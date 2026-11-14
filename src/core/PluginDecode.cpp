#include "PluginScan.h"
#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace UnifiedMusic {
namespace {
using Bytes = PluginBytes;
std::string Lower(std::string s) {
    for (auto& c : s) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return s;
}
std::uint32_t U32(Bytes b, size_t i) {
    if (i > b.size() || b.size()-i < 4) throw std::runtime_error("truncated uint32");
    return b[i] | (std::uint32_t(b[i+1])<<8) | (std::uint32_t(b[i+2])<<16) | (std::uint32_t(b[i+3])<<24);
}
std::string String(Bytes b) {
    auto end = std::find(b.begin(), b.end(), 0);
    return {b.begin(), end};
}
FormKey Resolve(std::uint32_t id, const std::vector<std::string>& masters, const std::string& own) {
    auto index=id>>24;
    if (!id || index>masters.size()) throw std::runtime_error("invalid music form reference");
    return {index==masters.size()?own:masters[index],id&0xffffff};
}
}

void PluginScan::Read(std::string own, const ReadRecords& read) {
    own=Lower(std::move(own));
    if(own.empty() || own.find_first_of("/\\:")!=std::string::npos)
        throw std::runtime_error("invalid plugin filename");
    std::vector<std::string> masters;
    bool sawHeader=false;
    // Commit only after the complete file passes structural validation.
    std::vector<MusicRecord> pending;
    std::vector<TrackRecord> trackPending;
    std::vector<MappingRecord> mappingPending;
    std::map<FormKey, AreaKind> areaPending;
    std::map<FormKey,std::set<std::optional<FormKey>>> parentsPending;
    std::set<FormKey> deletedAreaPending, mappedPending, nestedPending;
    std::vector<ScriptProperties> scriptPending;
    std::vector<Definition> definitionPending;
    read([&](const PluginRecord& input) {
        const auto& sig=input.type;
        const auto flags=input.flags, id=input.id;
        const auto& subs=input.fields;
        if(!sawHeader && sig!="TES4") throw std::runtime_error("missing TES4 header");
        if(sig=="TES4") {
            if(sawHeader) throw std::runtime_error("duplicate or misplaced TES4 header");
            sawHeader=true;
            for(auto& [s,v]:subs) if(s=="MAST") {
                auto master=Lower(String(v));
                if(master.empty() || master.find_first_of("/\\:")!=std::string::npos) throw std::runtime_error("invalid master filename");
                masters.push_back(master);
            }
        } else if (!(flags&(0x4000|0x1000))) {
            if(PluginRecordNeedsID(sig) && sig!="MUST") {
                Definition definition{Resolve(id,masters,own),own,{},sig,(flags&0x20)!=0};
                for(const auto& [s,v]:subs) if(s=="EDID") definition.editorID=String(v);
                definitionPending.push_back(std::move(definition));
            }
            if(sig=="MUSC") {
                MusicRecord record{Resolve(id,masters,own),own}; record.deleted=(flags&0x20)!=0;
                record.dependencies=masters;
                for(auto& [s,v]:subs) {
                    if(s=="EDID") record.editorID=String(v);
                    else if(s=="TNAM") {
                        if(v.size()%4) throw std::runtime_error("unaligned music track array");
                        for(size_t j=0;j<v.size();j+=4) record.tracks.push_back(Resolve(U32(v,j),masters,own));
                    } else {
                        if(s=="FNAM" && v.size()==4) record.oneShot=(U32(v,0)&1)!=0;
                        record.settings+=s;
                        auto sz=static_cast<std::uint32_t>(v.size());
                        record.settings.append(reinterpret_cast<char*>(&sz),4);
                        record.settings.append(reinterpret_cast<const char*>(v.data()),v.size());
                    }
                }
                pending.push_back(std::move(record));
            }
            if(sig=="MUST") {
                TrackRecord record{Resolve(id,masters,own),own,masters};
                record.deleted=(flags&0x20)!=0;
                const auto semanticFlags=flags&~0x40000u;
                record.metadata.append(reinterpret_cast<const char*>(&semanticFlags),4);
                bool audio=false, single=false;
                unsigned cnams=0, anams=0;
                std::optional<std::uint32_t> conditionCount;
                for(auto& [s,v]:subs) {
                    if(s!="CTDA" && s!="CITC") {
                        record.metadata+=s;
                        const auto size=static_cast<std::uint32_t>(v.size());
                        record.metadata.append(reinterpret_cast<const char*>(&size),4);
                        auto value=std::string(reinterpret_cast<const char*>(v.data()),v.size());
                        if(s=="ANAM" || s=="BNAM") {
                            value=Lower(std::move(value));
                            std::ranges::replace(value,'/','\\');
                        }
                        record.metadata+=value;
                    }
                    if(s=="CTDA") {
                        record.conditions.emplace_back(v.begin(),v.end());
                        if(v.size()!=32) record.invalid=true;
                    }
                    if(s=="CNAM") { ++cnams; single=v.size()==4 && U32(v,0)==0x6ed7e048; }
                    if(s=="ANAM") { ++anams; record.audioPath=String(v); audio=!record.audioPath.empty(); }
                    if(s=="BNAM") record.finalePath=String(v);
                    if(s=="CITC") {
                        if(v.size()!=4 || conditionCount) record.invalid=true;
                        else conditionCount=U32(v,0);
                    }
                    // Palette members have another owner. Conservatively
                    // protect every referenced child, including nested palettes.
                    if(s=="SNAM") {
                        if(v.size()%4) throw std::runtime_error("unaligned nested music track array");
                        for(size_t j=0;j<v.size();j+=4) if(U32(v,j)) nestedPending.insert(Resolve(U32(v,j),masters,own));
                    }
                }
                record.singleAudio=audio && single && cnams==1 && anams==1;
                if(conditionCount && *conditionCount!=record.conditions.size()) record.invalid=true;
                trackPending.push_back(std::move(record));
            }
            if(sig=="CELL" || sig=="WRLD" || sig=="LCTN" || sig=="REGN") {
                const auto key=Resolve(id,masters,own);
                auto kind=sig=="CELL"?AreaKind::Cell:sig=="WRLD"?AreaKind::Worldspace:sig=="LCTN"?AreaKind::Location:AreaKind::Region;
                if(key.plugin==own) areaPending[key]=kind;
                if(flags&0x20) deletedAreaPending.insert(key);
                const auto musicField=sig=="CELL"?"XCMO":sig=="WRLD"?"ZNAM":sig=="LCTN"?"NAM1":"RDMO";
                MappingRecord mapping{{kind,key},own,{},(flags&0x20)!=0};
                unsigned fields=0;
                for(auto& [s,v]:subs) if(s==musicField) {
                    if(++fields>1 || v.size()!=4) mapping.invalid=true;
                    if(v.size()==4 && U32(v,0)) {
                        mapping.music=Resolve(U32(v,0),masters,own);
                        mappedPending.insert(*mapping.music);
                    }
                }
                // REGN sound entries have additional ordering/override rules.
                // Keep their ownership protection, but never flatten them.
                if(kind!=AreaKind::Region) mappingPending.push_back(std::move(mapping));
                if(sig=="LCTN") {
                    std::optional<FormKey> parent;
                    unsigned count=0;
                    for(const auto& [s,v]:subs) if(s=="PNAM") {
                        if(++count>1 || v.size()!=4) throw std::runtime_error("invalid location parent");
                        if(U32(v,0)) parent=Resolve(U32(v,0),masters,own);
                    }
                    parentsPending[key].insert(parent);
                }
            }
            for(auto& [s,v]:subs) if(s=="VMAD" && !(flags&0x20)) {
                ScriptProperties script{own,masters,{v.begin(),v.end()}};
                DecodeScriptProperties(script,sig);
                scriptPending.push_back(std::move(script));
            }
        }
    });
    if(!sawHeader) throw std::runtime_error("missing TES4 header");
    dependencies_[own]=masters;
    definitions_.insert(definitions_.end(),std::make_move_iterator(definitionPending.begin()),std::make_move_iterator(definitionPending.end()));
    for(auto& r:pending) music_[r.key].push_back(std::move(r));
    for(auto& r:trackPending) tracks_[r.key].push_back(std::move(r));
    for(auto& r:mappingPending) mappings_[r.area.form].push_back(std::move(r));
    areaForms_.insert(areaPending.begin(),areaPending.end());
    for(const auto& [key,parents]:parentsPending) locationParents_[key].insert(parents.begin(),parents.end());
    deletedAreas_.insert(deletedAreaPending.begin(),deletedAreaPending.end());
    mappedTypes_.insert(mappedPending.begin(),mappedPending.end());
    nestedTracks_.insert(nestedPending.begin(),nestedPending.end());
    scripts_.insert(scripts_.end(),std::make_move_iterator(scriptPending.begin()),std::make_move_iterator(scriptPending.end()));
}

}
