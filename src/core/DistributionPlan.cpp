#include "PluginScan.h"
#include <SimpleIni.h>
#include <algorithm>
#include <charconv>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string_view>

namespace UnifiedMusic {
namespace {
std::string Lower(std::string text) {
    for(auto& c:text) if(c>='A' && c<='Z') c+=32;
    return text;
}
std::string_view Trim(std::string_view text) {
    const auto first=text.find_first_not_of(" \t\r\n");
    if(first==text.npos) return {};
    return text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);
}
// SimpleIni handles sections, repeated keys, BOMs and multiline values. Accept
// whitespace-delimited end comments too; never truncate a filename at an
// embedded '#' or ';' without a whitespace delimiter.
std::string CleanValue(std::string_view value) {
    std::string result;
    while(!value.empty()) {
        const auto end=value.find('\n');
        auto line=value.substr(0,end);
        for(std::size_t i=0;i<line.size();++i)
            if((line[i]=='#' || line[i]==';') && (i==0 || line[i-1]==' ' || line[i-1]=='\t')) {
                line=line.substr(0,i);break;
            }
        if(!result.empty()) result+=' ';
        result+=Trim(line);
        if(end==value.npos) break;
        value.remove_prefix(end+1);
    }
    return result;
}
std::optional<FormKey> TrackKey(std::string_view text) {
    const auto split=text.find('~');
    if(split==text.npos || text.find('~',split+1)!=text.npos) return {};
    auto number=Trim(text.substr(0,split));
    auto plugin=Lower(std::string(Trim(text.substr(split+1))));
    if(number.starts_with("0x") || number.starts_with("0X")) number.remove_prefix(2);
    if(number.empty() || number.size()>8) return {};
    std::uint32_t id=0;
    const auto parsed=std::from_chars(number.data(),number.data()+number.size(),id,16);
    if(parsed.ec!=std::errc{} || parsed.ptr!=number.data()+number.size() ||
       !id || id>0xffffff || plugin.empty() || plugin.find_first_of("/\\:;#=\"\n")!=plugin.npos ||
       !(plugin.ends_with(".esm") || plugin.ends_with(".esp") || plugin.ends_with(".esl"))) return {};
    return FormKey{plugin,id};
}
}

void PluginScan::ReadConfigs(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> files;
    for(const auto& entry:std::filesystem::directory_iterator(directory))
        if(entry.is_regular_file() && Lower(entry.path().filename().string()).ends_with("_mus.ini")) files.push_back(entry.path());
    std::sort(files.begin(),files.end());
    for(const auto& file:files) ReadConfig(file);
}

void PluginScan::ReadConfig(const std::filesystem::path& path) {
    const auto source=path.filename().string();
    const auto fail=[&](std::size_t line,std::string_view reason) {
        configErrors_.push_back("CONFIG_UNSUPPORTED "+source+":"+std::to_string(line)+" "+std::string(reason));
    };
    std::ifstream stream(path,std::ios::binary|std::ios::ate);
    if(!stream) {fail(0,"cannot open file");return;}
    const auto size=stream.tellg();
    if(size<0 || size>4*1024*1024) {fail(0,"file size limit");return;}
    std::string content(static_cast<std::size_t>(size),'\0');
    stream.seekg(0);
    if(!stream.read(content.data(),size)) {fail(0,"short read");return;}
    if(content.find('\0')!=content.npos) {fail(0,"NUL/UTF-16 is unsupported; use UTF-8");return;}
    CSimpleIniA ini(true,true,true);
    if(ini.LoadData(content.data(),content.size())<0) {fail(0,"invalid INI");return;}
    std::vector<DistributionEntry> pending;
    CSimpleIniA::TNamesDepend keys;
    ini.GetAllKeys("General",keys);
    for(const auto& k:keys) {
        auto target=Trim(k.pItem);
        const bool clear=target.ends_with('!');
        if(clear) {target.remove_suffix(1);target=Trim(target);}
        if(target.empty() || target.find_first_of("!;#~\" \\/:\t\r\n")!=target.npos) {fail(0,"invalid MusicType EditorID");return;}
        CSimpleIniA::TNamesDepend entries;ini.GetAllValues("General",k.pItem,entries);
        for(const auto& v:entries) {
            DistributionEntry entry{std::string(target),source+" [General] "+k.pItem,{},clear};
            const auto cleaned=CleanValue(v.pItem);
            auto values=Trim(cleaned);
            if(!values.empty()) for(;;) {
                const auto comma=values.find(',');
                const auto key=TrackKey(Trim(values.substr(0,comma)));
                if(!key) {fail(0,"expected local hex FormID~plugin filename for "+std::string(k.pItem));return;}
                entry.tracks.push_back(*key);
                if(comma==values.npos) break;
                values.remove_prefix(comma+1);
            }
            pending.push_back(std::move(entry));
        }
    }
    std::vector<AssignmentEntry> assignments;
    for(const auto& [section,kind]:{std::pair{"Location",AreaKind::Location},std::pair{"Region",AreaKind::Region}}) {
        keys.clear();ini.GetAllKeys(section,keys);
        for(const auto& key:keys) {
            CSimpleIniA::TNamesDepend values;ini.GetAllValues(section,key.pItem,values);
            values.sort(CSimpleIniA::Entry::LoadOrder());
            for(const auto& value:values) {
                auto music=CleanValue(value.pItem);
                if(music.empty()) {fail(0,"empty MusicType assignment");return;}
                assignments.push_back({kind,std::string(Trim(key.pItem)),std::move(music),source+" ["+section+"] "+key.pItem});
            }
        }
    }
    // MTD reads only these three sections; unrelated metadata is ignored.
    distributionEntries_.insert(distributionEntries_.end(),std::make_move_iterator(pending.begin()),std::make_move_iterator(pending.end()));
    assignmentEntries_[path]=std::move(assignments);
}

bool PluginScan::PlanDistribution(MergePlan& plan) const {
    plan.diagnostics.insert(plan.diagnostics.end(),configErrors_.begin(),configErrors_.end());
    bool valid=configErrors_.empty();
    std::map<FormKey,std::set<FormKey>> pending;
    for(const auto& entry:distributionEntries_) {
        const auto fail=[&](const std::string& reason) {
            valid=false;
            plan.diagnostics.push_back("CONFIG_UNSUPPORTED "+entry.source+" "+reason);
        };
        const auto target=ResolveConfigForm(entry.target,"MUSC");
        if(!target) {fail("missing/deleted/renamed/ambiguous existing MusicType "+entry.target);continue;}
        const auto& key=*target;
        if(entry.clear) plan.clearedTypes.insert(key);
        const std::set<FormKey> unique(entry.tracks.begin(),entry.tracks.end());
        for(const auto& track:unique) {
            const auto found=tracks_.find(track);
            if(found==tracks_.end() || !std::ranges::any_of(found->second,[&](const auto& r){return r.source==track.plugin;}) ||
               std::ranges::any_of(found->second,[](const auto& r){return r.deleted || r.invalid;})) {
                // MTD ignores unresolved tracks. Preserve that useful behavior
                // for optional/stale references, but expose every skipped pair.
                plan.diagnostics.push_back("CONFIG_SKIP "+entry.source+" missing/deleted/invalid MUST "+ToString(track));continue;
            }
            // As in MTD, explicit membership is authoritative even when an ESP
            // removed it. The track's winning playback conditions stay intact.
            pending[key].insert(track);
        }
    }
    // Alphabetical files and last assignment per area, matching MTD. Keep this
    // independent of inspector argument order and plugin traversal order.
    for(const auto& [file,entries]:assignmentEntries_) for(const auto& entry:entries) {
        const auto area=ResolveConfigForm(entry.area,entry.kind==AreaKind::Location?"LCTN":"REGN");
        const auto music=ResolveConfigForm(entry.music,"MUSC");
        if(!area || !music) {
            valid=false;plan.diagnostics.push_back("CONFIG_UNSUPPORTED "+entry.source+" missing/deleted/renamed/ambiguous area or MusicType");continue;
        }
        plan.assignments[{entry.kind,*area}]=*music;
    }
    if(!valid) {
        plan.clearedTypes.clear();plan.assignments.clear();
        plan.diagnostics.push_back("CONFIG_HANDOFF automatic writes paused; original MTD or an authored patch required for these configurations");
        return false;
    }
    for(const auto& [key,tracks]:pending) plan.distributions[key].assign(tracks.begin(),tracks.end());
    return true;
}
}
