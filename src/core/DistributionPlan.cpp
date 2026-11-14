#include "PluginScan.h"
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
    const auto first=text.find_first_not_of(" \t\r");
    if(first==text.npos) return {};
    return text.substr(first,text.find_last_not_of(" \t\r")-first+1);
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
    if(content.starts_with("\xef\xbb\xbf")) content.erase(0,3);
    if(content.find('\0')!=content.npos) {fail(0,"NUL/UTF-16 is unsupported; use UTF-8");return;}
    std::vector<DistributionEntry> pending;
    std::string section;
    std::size_t number=0;
    for(std::size_t offset=0;offset<content.size();) {
        ++number;
        const auto end=content.find('\n',offset);
        auto line=Trim(std::string_view(content).substr(offset,end==content.npos?content.size()-offset:end-offset));
        offset=end==content.npos?content.size():end+1;
        if(line.empty() || line.front()==';' || line.front()=='#') continue;
        if(line.size()>256*1024) {fail(number,"line size limit");return;}
        if(line.front()=='[') {
            if(line.back()!=']') {fail(number,"malformed section");return;}
            section=Lower(std::string(Trim(line.substr(1,line.size()-2))));
            if(section!="general") {fail(number,"only additive [General] is supported");return;}
            continue;
        }
        const auto equal=line.find('=');
        if(section!="general" || equal==line.npos) {fail(number,"expected [General] target = tracks");return;}
        const auto target=Trim(line.substr(0,equal));
        if(target.empty() || target.find_first_of("!;#~\" \\/:\t")!=target.npos) {fail(number,"invalid target or destructive clear (!)");return;}
        DistributionEntry entry{std::string(target),source+":"+std::to_string(number),{}};
        auto values=Trim(line.substr(equal+1));
        // SimpleIni's whole-line comments are accepted. Inline comments and
        // multiline values are not silently truncated into a different rule.
        for(;;) {
            const auto comma=values.find(',');
            const auto key=TrackKey(Trim(values.substr(0,comma)));
            if(!key) {fail(number,"expected local hex FormID~plugin filename");return;}
            entry.tracks.push_back(*key);
            if(comma==values.npos) break;
            values.remove_prefix(comma+1);
        }
        pending.push_back(std::move(entry));
    }
    distributionEntries_.insert(distributionEntries_.end(),std::make_move_iterator(pending.begin()),std::make_move_iterator(pending.end()));
}

bool PluginScan::PlanDistribution(MergePlan& plan) const {
    plan.diagnostics.insert(plan.diagnostics.end(),configErrors_.begin(),configErrors_.end());
    std::map<std::string,std::set<FormKey>> targets;
    for(const auto& [key,versions]:music_) for(const auto& record:versions)
        if(!record.editorID.empty()) targets[Lower(record.editorID)].insert(key);
    bool valid=configErrors_.empty();
    std::map<FormKey,std::set<FormKey>> pending;
    for(const auto& entry:distributionEntries_) {
        const auto fail=[&](const std::string& reason) {
            valid=false;
            plan.diagnostics.push_back("CONFIG_UNSUPPORTED "+entry.source+" "+reason);
        };
        const auto target=targets.find(Lower(entry.target));
        if(target==targets.end() || target->second.size()!=1) {fail("missing/ambiguous existing MusicType "+entry.target);continue;}
        const auto& key=*target->second.begin();
        const auto& versions=music_.at(key);
        if(!std::ranges::any_of(versions,[&](const auto& r){return r.source==key.plugin;}) ||
           std::ranges::any_of(versions,[&](const auto& r){return r.deleted || Lower(r.editorID)!=Lower(entry.target);})) {
            fail("deleted or renamed MusicType "+entry.target);continue;
        }
        const std::set<FormKey> unique(entry.tracks.begin(),entry.tracks.end());
        for(const auto& track:unique) {
            const auto found=tracks_.find(track);
            if(found==tracks_.end() || !std::ranges::any_of(found->second,[&](const auto& r){return r.source==track.plugin;}) ||
               std::ranges::any_of(found->second,[](const auto& r){return r.deleted || r.invalid;})) {
                // MTD ignores unresolved tracks. Preserve that useful behavior
                // for optional/stale references, but expose every skipped pair.
                plan.diagnostics.push_back("CONFIG_SKIP "+entry.source+" missing/deleted/invalid MUST "+ToString(track));continue;
            }
            bool removed=false;
            for(const auto& current:versions) for(const auto& prior:versions)
                if(DependsOn(current.source,prior.source) &&
                   std::ranges::find(prior.tracks,track)!=prior.tracks.end() &&
                   std::ranges::find(current.tracks,track)==current.tracks.end()) removed=true;
            if(removed) {
                plan.diagnostics.push_back("CONFIG_SKIP "+entry.source+" dependent playlist removal "+ToString(track));continue;
            }
            pending[key].insert(track);
        }
    }
    if(!valid) {
        // A clear, new type, location/region rule or ambiguous target must not
        // be half-applied while automatic restoration fights its intent.
        plan.diagnostics.push_back("CONFIG_HANDOFF automatic writes paused; original MTD or an authored patch required for these configurations");
        return false;
    }
    for(const auto& [key,tracks]:pending) plan.distributions[key].assign(tracks.begin(),tracks.end());
    return true;
}
}
