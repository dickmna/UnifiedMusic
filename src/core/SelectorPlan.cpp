#include "PluginScan.h"
#include <json/json.h>
#include <algorithm>
#include <charconv>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace UnifiedMusic {
namespace {
constexpr const char* fields[]{"worldspaces","cells","locations","locationKeywords","combatTarget","combatTargetKeywords"};
constexpr const char* types[]{"WRLD","CELL","LCTN","KYWD","NPC_","KYWD"};
std::string Lower(std::string s) {for(auto& c:s) if(c>='A'&&c<='Z') c+=32;return s;}
}
void PluginScan::ReadSelectors(const std::filesystem::path& directory) {
    if(!std::filesystem::exists(directory)) return;
    std::vector<std::filesystem::path> files;
    for(const auto& entry:std::filesystem::directory_iterator(directory))
        if(entry.is_regular_file() && Lower(entry.path().extension().string())==".json") files.push_back(entry.path());
    std::sort(files.begin(),files.end());for(const auto& file:files) ReadSelector(file);
}
void PluginScan::ReadSelector(const std::filesystem::path& path) {
    try {
        if(std::filesystem::file_size(path)>4*1024*1024) throw std::runtime_error("file exceeds 4 MiB");
        std::ifstream file(path,std::ios::binary);if(!file) throw std::runtime_error("cannot open JSON");
        Json::CharReaderBuilder builder;builder["collectComments"]=false;builder["allowComments"]=true;
        builder["rejectDupKeys"]=true;builder["failIfExtra"]=true;builder["stackLimit"]=32;
        Json::Value root;std::string error;
        if(!Json::parseFromStream(builder,file,&root,&error)) throw std::runtime_error("invalid JSON");
        if(!root.isObject() ||
            !root["combatMusic"].isArray() || root["combatMusic"].size()>4096) throw std::runtime_error("expected combatMusic array");
        std::vector<RawSelector> pending;
        for(const auto& entry:root["combatMusic"]) {
            if(!entry.isObject() || !entry["isCombatMusic"].isBool() || !entry["newMusic"].isString() || entry["newMusic"].asString().empty())
                throw std::runtime_error("expected isCombatMusic and newMusic");
            // CME reads its named fields and ignores descriptive metadata.
            RawSelector rule{entry["isCombatMusic"].asBool(),entry["newMusic"].asString(),path.filename().string(),{}};
            for(int kind=0;kind<6;++kind) if(entry.isMember(fields[kind])) {
                const auto& filter=entry[fields[kind]];
                if(!filter.isObject() || !filter["AND"].isBool() || !filter["forms"].isArray() || filter["forms"].size()>4096)
                    throw std::runtime_error("invalid selector filter");
                // Upstream does not install empty filters. A completely empty
                // rule has rank zero, so omit it rather than making it global.
                if(filter["forms"].empty()) continue;
                RawFilter f{static_cast<SelectorKind>(kind),filter["AND"].asBool(),{}};
                for(const auto& form:filter["forms"]) {
                    if(!form.isString() || form.asString().empty() || form.asString().size()>512 || form.asString().find('\0')!=std::string::npos)
                        throw std::runtime_error("invalid form identifier");
                    f.forms.push_back(form.asString());
                }
                rule.filters.push_back(std::move(f));
            }
            if(!rule.filters.empty()) pending.push_back(std::move(rule));
        }
        selectorInputs_.insert(selectorInputs_.end(),std::make_move_iterator(pending.begin()),std::make_move_iterator(pending.end()));
    } catch(const std::exception& e) {configErrors_.push_back(path.filename().string()+": "+e.what());}
}
std::optional<FormKey> PluginScan::ResolveConfigForm(const std::string& token,const std::string& type) const {
        std::optional<FormKey> explicitKey;
        const auto delimiter=token.find('|');
        if(delimiter!=std::string::npos) {
            auto name=Lower(token.substr(0,delimiter));auto id=token.substr(delimiter+1);
            if(name.empty() || name.find_first_of("/\\:")!=std::string::npos ||
                !(name.ends_with(".esp")||name.ends_with(".esm")||name.ends_with(".esl")) ||
                !(id.starts_with("0x")||id.starts_with("0X"))) return {};
            std::uint32_t value{};const auto conversion=std::from_chars(id.data()+2,id.data()+id.size(),value,16);
            if(conversion.ec!=std::errc{} || conversion.ptr!=id.data()+id.size() || !value || value>0xffffff) return {};
            explicitKey=FormKey{name,value};
        }
        std::set<FormKey> matches;
        for(const auto& d:definitions_) if(d.type==type && (explicitKey?d.key==*explicitKey:Lower(d.editorID)==Lower(token))) matches.insert(d.key);
        if(matches.size()!=1) return {};
        const auto& key=*matches.begin();bool origin=false;
        for(const auto& d:definitions_) if(d.key==key) {
            if(d.deleted || d.type!=type || (!explicitKey && Lower(d.editorID)!=Lower(token))) return {};
            origin|=d.source==key.plugin;
        }
        return origin?std::optional<FormKey>(key):std::nullopt;
}
bool PluginScan::PlanSelectors(MergePlan& plan) const {
    std::vector<SelectorRule> resolved;
    for(const auto& raw:selectorInputs_) {
        const auto music=ResolveConfigForm(raw.music,"MUSC");bool valid=music.has_value();
        SelectorRule rule{raw.combat,music.value_or(FormKey{}),{},raw.source};
        for(const auto& rawFilter:raw.filters) {
            SelectorFilter f{rawFilter.kind,rawFilter.required,{}};
            for(const auto& name:rawFilter.forms) {
                const auto form=ResolveConfigForm(name,types[static_cast<int>(f.kind)]);
                if(!form) {valid=false;break;}
                f.forms.push_back(*form);
            }
            rule.filters.push_back(std::move(f));
        }
        if(!valid) {
            auto diagnostics=std::move(plan.diagnostics);plan={};plan.diagnostics=std::move(diagnostics);
            plan.diagnostics.push_back("CONFIG_HANDOFF "+raw.source+" unresolved, deleted, renamed or ambiguous CME form; all automatic writes paused");
            return false;
        }
        resolved.push_back(std::move(rule));
    }
    plan.selectors=std::move(resolved);return true;
}
}
