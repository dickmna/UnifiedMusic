#include "PluginScan.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <span>
#include <stdexcept>
#include <zlib.h>

namespace UnifiedMusic {
namespace {
using Bytes = std::span<const std::uint8_t>;
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
std::vector<std::pair<std::string, Bytes>> Subs(Bytes b) {
    std::vector<std::pair<std::string, Bytes>> result;
    size_t i=0; std::uint32_t extended=0; bool hasExtended=false;
    while (i < b.size()) {
        if (b.size()-i < 6) throw std::runtime_error("truncated subrecord");
        std::string sig(reinterpret_cast<const char*>(b.data()+i),4);
        size_t n=b[i+4] | (unsigned(b[i+5])<<8); i+=6;
        if (sig=="XXXX") {
            if(n!=4 || hasExtended) throw std::runtime_error("invalid XXXX");
            extended=U32(b,i); hasExtended=true; i+=4; continue;
        }
        if (hasExtended) { n=extended; hasExtended=false; }
        if (n>b.size()-i) throw std::runtime_error("subrecord exceeds record");
        result.emplace_back(sig,b.subspan(i,n)); i+=n;
    }
    if(hasExtended) throw std::runtime_error("orphan XXXX");
    return result;
}
FormKey Resolve(std::uint32_t id, const std::vector<std::string>& masters, const std::string& own) {
    auto index=id>>24;
    if (!id || index>masters.size()) throw std::runtime_error("invalid music form reference");
    return {index==masters.size()?own:masters[index],id&0xffffff};
}
bool IsBase(const std::string& name) {
    static const std::set<std::string> names{"skyrim.esm","update.esm","dawnguard.esm","hearthfires.esm","dragonborn.esm"};
    return names.contains(name);
}
bool Contains(const std::vector<FormKey>& v,const FormKey& key) { return std::find(v.begin(),v.end(),key)!=v.end(); }
}

bool PluginScan::Ambient(const MusicRecord& r) const {
    if(!IsBase(r.key.plugin) || r.oneShot || r.source!=r.key.plugin) return false;
    auto name=Lower(r.editorID);
    for (auto term : {"combat","battle","boss","silent","silence","blank","reveal","discov","stinger","finale","event","reward"})
        if(name.find(term)!=std::string::npos) return false;
    if(name.find("test")!=std::string::npos && !std::ranges::any_of(mappings_,[&](const auto& entry) {
        return std::ranges::any_of(entry.second,[&](const auto& m) {
            return IsBase(m.source) && m.source==m.area.form.plugin && !m.deleted && !m.invalid && m.music==r.key;
        });
    })) return false;
    for (auto term : {"explore","town","dungeon","tavern"})
        if(name.find(term)!=std::string::npos) return true;
    return false;
}

std::string ToString(const FormKey& key) { return key.plugin + ":" + std::to_string(key.id); }

void PluginScan::Read(const std::filesystem::path& path) {
    std::ifstream stream(path,std::ios::binary|std::ios::ate);
    if(!stream) throw std::runtime_error("cannot open plugin: "+path.string());
    auto length=stream.tellg();
    if(length<24 || length>std::streamoff(2ULL<<30)) throw std::runtime_error("invalid plugin size");
    std::vector<std::uint8_t> data(static_cast<size_t>(length)); stream.seekg(0);
    if(!stream.read(reinterpret_cast<char*>(data.data()),length)) throw std::runtime_error("short plugin read");
    if(std::memcmp(data.data(),"TES4",4)!=0) throw std::runtime_error("missing TES4 header");
    std::string own=Lower(path.filename().string());
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
    std::function<void(size_t,size_t,unsigned)> walk = [&](size_t start,size_t end,unsigned depth) {
        if(depth>32) throw std::runtime_error("plugin group depth exceeded");
        for(size_t p=start;p<end;) {
            if(end-p<24) throw std::runtime_error("truncated record header");
            std::string sig(reinterpret_cast<char*>(data.data()+p),4);
            size_t n=U32(data,p+4);
            if(sig=="GRUP") {
                if(n<24 || n>end-p) throw std::runtime_error("invalid group boundary");
                walk(p+24,p+n,depth+1); p+=n; continue;
            }
            if(n>end-p-24) throw std::runtime_error("invalid record boundary");
            auto flags=U32(data,p+8), id=U32(data,p+12);
            Bytes body=Bytes(data).subspan(p+24,n);
            std::vector<std::uint8_t> expanded;
            if(flags&0x40000) {
                auto expected=U32(body,0);
                if(expected>128*1024*1024) throw std::runtime_error("record decompression limit exceeded");
                expanded.resize(expected); uLongf size=expected;
                if(uncompress(expanded.data(),&size,body.data()+4,static_cast<uLong>(body.size()-4))!=Z_OK || size!=expected)
                    throw std::runtime_error("record decompression failed");
                body=expanded;
            }
            auto subs=Subs(body);
            if(sig=="TES4") {
                if(sawHeader || p!=0) throw std::runtime_error("duplicate or misplaced TES4 header");
                sawHeader=true;
                for(auto& [s,v]:subs) if(s=="MAST") {
                    auto master=Lower(String(v));
                    if(master.empty() || master.find_first_of("/\\:")!=std::string::npos) throw std::runtime_error("invalid master filename");
                    masters.push_back(master);
                }
            } else if (!(flags&(0x4000|0x1000))) {
                if(sig=="MUSC" || sig=="WRLD" || sig=="CELL" || sig=="LCTN" || sig=="REGN" || sig=="KYWD" || sig=="NPC_" || sig=="QUST" || sig=="GLOB") {
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
            p+=24+n;
        }
    };
    walk(0,data.size(),0);
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

bool PluginScan::ScriptReferences(const FormKey& key,bool modsOnly) const {
    for(const auto& script:scripts_) {
        if(modsOnly && IsBase(script.source)) continue;
        if(script.parsed) {if(script.references.contains(key)) return true;continue;}
        auto it=std::find(script.masters.begin(),script.masters.end(),key.plugin);
        size_t index=it-script.masters.begin();
        if(it==script.masters.end() && script.source!=key.plugin) continue;
        auto raw=key.id | (static_cast<std::uint32_t>(index)<<24);
        std::array<std::uint8_t,4> needle{std::uint8_t(raw),std::uint8_t(raw>>8),std::uint8_t(raw>>16),std::uint8_t(raw>>24)};
        if(std::search(script.bytes.begin(),script.bytes.end(),needle.begin(),needle.end())!=script.bytes.end()) return true;
    }
    return false;
}

bool PluginScan::DependsOn(const std::string& source,const std::string& dependency) const {
    std::set<std::string> visited{source};
    auto found=dependencies_.find(source);
    if(found==dependencies_.end()) return false;
    auto pending=found->second;
    while(!pending.empty()) {
        const auto name=pending.back(); pending.pop_back();
        if(name==dependency) return true;
        if(!visited.insert(name).second) continue;
        found=dependencies_.find(name);
        if(found!=dependencies_.end()) pending.insert(pending.end(),found->second.begin(),found->second.end());
    }
    return false;
}

std::set<FormKey> PluginScan::ProtectedTracks(const FormKey& excludedType,const MergePlan& plan) const {
    auto protectedTracks=nestedTracks_;
    const auto target=music_.find(excludedType);
    const bool ambientTarget=target!=music_.end() && std::ranges::any_of(target->second,[&](const auto& r){return Ambient(r);});
    for(const auto& [key,records]:music_) {
        if(key==excludedType) continue;
        const auto origin=std::find_if(records.begin(),records.end(),[&](const auto& r){return r.source==key.plugin;});
        const bool ordinaryAmbient=origin!=records.end() && Ambient(*origin);
        // Ordinary ambient lists can share tracks across biomes. A map binding
        // alone is not exclusive ownership, but must not leak into combat.
        for(const auto& r:records) if(!ordinaryAmbient || plan.scriptBound.contains(key) || (!ambientTarget && mappedTypes_.contains(key)) || r.oneShot) {
            protectedTracks.insert(r.tracks.begin(),r.tracks.end());
        }
    }
    // Configuration-only Boss/custom owners count just as plugin TNAM owners.
    // For inferred condition edits, even sharing between ambient lists is
    // protected: a restriction for one biome must not change another list.
    for(const auto& [key,tracks]:plan.distributions) if(key!=excludedType)
        protectedTracks.insert(tracks.begin(),tracks.end());
    for(const auto& [key,records]:tracks_) {
        if(std::ranges::any_of(records,[](const auto& r){return r.deleted || r.invalid;})) protectedTracks.insert(key);
    }
    return protectedTracks;
}

std::vector<const TrackRecord*> PluginScan::EffectiveTracks(const FormKey& key) const {
    const auto found=tracks_.find(key);
    if(found==tracks_.end() || !std::ranges::any_of(found->second,[&](const auto& r){return r.source==key.plugin;}) ||
       std::ranges::any_of(found->second,[](const auto& r){return r.deleted || r.invalid;})) return {};
    std::vector<const TrackRecord*> result;
    for(const auto& record:found->second)
        if(!std::ranges::any_of(found->second,[&](const auto& later){return DependsOn(later.source,record.source);})) result.push_back(&record);
    std::ranges::sort(result,{},[](const auto* r){return r->source;});
    return result;
}

bool PluginScan::ValidParameter(const ConditionTerm& term) const {
    if(term.function==18 || term.function==520 ||
       (term.function==707 && term.parameter==FormKey{"skyrim.esm",0x35d59})) return true;
    const auto expected=term.function==67?"CELL":term.function==310?"WRLD":term.function==359?"LCTN":
        term.function==74?"GLOB":term.function==117?"REGN":(term.function==562 || term.function==707)?"KYWD":"QUST";
    bool exists=false;
    for(const auto& d:definitions_) if(d.key==term.parameter) {
        if(d.deleted || d.type!=expected) return false;
        if(d.source==d.key.plugin) exists=true;
    }
    return exists;
}

bool PluginScan::Within(const Area& child,const Area& parent) const {
    if(child.kind!=parent.kind) return false;
    auto current=child.form;
    std::set<FormKey> seen;
    for(unsigned depth=0;depth<64;++depth) {
        if(!seen.insert(current).second || deletedAreas_.contains(current)) return false;
        if(current==parent.form) return true;
        if(child.kind!=AreaKind::Location) return false;
        const auto it=locationParents_.find(current);
        if(it==locationParents_.end() || it->second.size()!=1 || !*it->second.begin()) return false;
        current=**it->second.begin();
    }
    return false;
}

MergePlan PluginScan::Plan() const {
    MergePlan plan;
    if(!PlanDistribution(plan) || !PlanSelectors(plan)) return plan;
    const auto unknown=std::ranges::count_if(scripts_,[](const auto& s){return !s.parsed;});
    if(unknown) plan.diagnostics.push_back("VMAD_FALLBACK "+std::to_string(unknown)+" unsupported property blocks retain conservative reference protection");
    // Complete this set before checking tracks shared with any other type.
    for(const auto& [key,chain]:music_) if(ScriptReferences(key)) plan.scriptBound.insert(key);
    for(const auto& rule:plan.selectors) plan.scriptBound.insert(rule.music);
    for(const auto& [key,chain]:music_) {
        const auto origin=std::find_if(chain.begin(),chain.end(),[&](const auto& r){return r.source==key.plugin;});
        if(origin==chain.end() || !Ambient(*origin)) continue;
        const auto& base=*origin;
        // Known public ambient lists retain their public role when referenced.
        bool protectedType=false;
        for(size_t i=0;i<chain.size();++i) {
            const auto& r=chain[i];
            if(r.deleted || r.settings!=base.settings) protectedType=true;
            for(const auto& track:base.tracks) if(!Contains(r.tracks,track)) protectedType=true;
            for(size_t j=0;j<chain.size();++j) {
                if(i==j) continue;
                const auto& prior=chain[j];
                if(!DependsOn(r.source,prior.source)) continue;
                for(const auto& track:prior.tracks) if(!Contains(r.tracks,track)) protectedType=true;
            }
        }
        if(protectedType) {
            plan.diagnostics.push_back("PRESERVE "+ToString(key)+" "+base.editorID+" (metadata change, deletion or track removal)");
            continue;
        }
        plan.allowed.insert(key);
        auto& additions=plan.additions[key];
        for(const auto& r:chain) {
            if(IsBase(r.source)) continue;
            for(const auto& track:r.tracks) {
                if(Contains(base.tracks,track) || Contains(additions,track)) continue;
                const auto found=tracks_.find(track);
                const bool invalid=found!=tracks_.end() && std::ranges::any_of(found->second,[](const auto& r){return r.deleted || r.invalid;});
                if(invalid || ScriptReferences(track)) {
                    plan.diagnostics.push_back("SKIP_TRACK "+ToString(track)+" -> "+ToString(key)+" (scripted, deleted or invalid track)");
                    continue;
                }
                additions.push_back(track);
            }
        }
    }
    InferCombat(plan);
    // A clear is an explicit complete playlist, not a missing contribution.
    // Resolve ordinary/Boss links first so clearing one owner cannot suppress
    // independent recovery in another; then exclude this owner from inference.
    for(const auto& type:plan.clearedTypes) {
        plan.additions.erase(type);
        plan.allowed.erase(type);
        if(plan.battleType && type==*plan.battleType) {
            plan.combatRules.clear();plan.battleMergeSafe=false;
        }
    }
    PlanAudio(plan);
    InferConditionRepairs(plan);
    InferMappings(plan);
    return plan;
}
}
