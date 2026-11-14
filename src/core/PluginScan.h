#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace UnifiedMusic {
struct FormKey {
    std::string plugin;
    std::uint32_t id{};
    auto operator<=>(const FormKey&) const = default;
};
struct MusicRecord {
    FormKey key;
    std::string source, editorID, settings;
    std::vector<FormKey> tracks;
    std::vector<std::string> dependencies;
    bool deleted{};
    bool oneShot{};
};
struct ScriptProperties {
    std::string source;
    std::vector<std::string> masters;
    std::vector<std::uint8_t> bytes;
    std::set<FormKey> references;
    bool parsed{};
};
void DecodeScriptProperties(ScriptProperties& script, const std::string& recordType);
enum class AreaKind { Cell, Worldspace, Location, Region };
struct Area {
    AreaKind kind;
    FormKey form;
    auto operator<=>(const Area&) const = default;
};
struct MappingRecord {
    Area area;
    std::string source;
    std::optional<FormKey> music;
    bool deleted{}, invalid{};
};
struct MappingRepair {
    Area area;
    FormKey music;
    std::vector<MappingRecord> versions;
};
struct AutoCombatRule {
    std::vector<Area> areas;
    std::vector<FormKey> tracks;
};
struct TrackRecord {
    FormKey key;
    std::string source;
    std::vector<std::string> masters;
    std::vector<std::vector<std::uint8_t>> conditions;
    bool singleAudio{}, deleted{}, invalid{};
    std::string metadata;
    std::string audioPath, finalePath;
};
struct ConditionTerm {
    std::uint32_t function{}, comparison{}, parameter2{};
    FormKey parameter;
    std::uint8_t opcode{};
    bool onPlayerReference{};
    bool isOR{};
    auto operator<=>(const ConditionTerm&) const = default;
};
struct ConditionVersion {
    std::string source;
    std::vector<ConditionTerm> conditions;
};
struct ConditionRepair {
    FormKey track;
    FormKey musicType;
    std::vector<Area> exclusions;
    std::vector<ConditionVersion> versions;
    std::vector<FormKey> requiredTracks;
    bool genericGuard{};
    bool isolate{}; // Replace only this playlist's reference with a private instance.
    std::vector<std::vector<ConditionTerm>> clauses; // AND of OR clauses, appended intact.
};
struct AudioCandidate {
    FormKey musicType, track;
    std::string audioPath, finalePath;
};
struct ConditionWitness {
    FormKey track;
    std::vector<ConditionVersion> versions;
    FormKey musicType;
};
struct SceneResource { FormKey musicType, track; std::string audioPath, finalePath; };
enum class SelectorKind { Worldspace, Cell, Location, LocationKeyword, Target, TargetKeyword };
struct SelectorFilter { SelectorKind kind; bool required{}; std::vector<FormKey> forms; };
struct SelectorRule { bool combat{}; FormKey music; std::vector<SelectorFilter> filters; std::string source; };
struct MergePlan {
    std::map<FormKey, std::vector<FormKey>> additions;
    std::map<FormKey, std::vector<FormKey>> distributions;
    std::set<FormKey> allowed;
    std::set<FormKey> scriptBound;
    std::optional<FormKey> battleType;
    std::vector<AutoCombatRule> combatRules;
    bool battleMergeSafe{};
    std::vector<ConditionRepair> conditionRepairs;
    std::vector<ConditionWitness> conditionWitnesses;
    std::vector<MappingRepair> mappingRepairs;
    std::vector<AudioCandidate> audioCandidates;
    std::vector<SceneResource> sceneResources;
    std::map<FormKey,std::set<FormKey>> sceneUses; // Only these uses depend on scene preparation.
    std::vector<SelectorRule> selectors;
    std::vector<std::string> diagnostics;
};
// Drop only scene-dependent actions; dedicated/dragon contributions can survive.
void DropSceneUses(MergePlan& plan, const FormKey& type);
class PluginScan {
public:
    void Read(const std::filesystem::path& path);
    void ReadConfig(const std::filesystem::path& path);
    void ReadConfigs(const std::filesystem::path& directory);
    void ReadSelector(const std::filesystem::path& path);
    void ReadSelectors(const std::filesystem::path& directory);
    MergePlan Plan() const;
    const auto& Records() const { return music_; }
    const auto& Tracks() const { return tracks_; }
private:
    bool ScriptReferences(const FormKey& key, bool modsOnly = false) const;
    bool DependsOn(const std::string& source, const std::string& dependency) const;
    std::set<FormKey> ProtectedTracks(const FormKey& excludedType, const MergePlan& plan) const;
    bool PlanDistribution(MergePlan& plan) const;
    void InferCombat(MergePlan& plan) const;
    void InferConditionRepairs(MergePlan& plan) const;
    void InferConditionsFor(MergePlan& plan, const FormKey& type, bool battle) const;
    void InferMappings(MergePlan& plan) const;
    void PlanAudio(MergePlan& plan) const;
    bool PlanSelectors(MergePlan& plan) const;
    std::map<FormKey, std::vector<MusicRecord>> music_;
    std::map<FormKey, std::vector<TrackRecord>> tracks_;
    std::map<FormKey, AreaKind> areaForms_;
    std::map<FormKey, std::vector<MappingRecord>> mappings_;
    std::set<FormKey> deletedAreas_, mappedTypes_, nestedTracks_;
    std::vector<ScriptProperties> scripts_;
    std::map<std::string, std::vector<std::string>> dependencies_;
    struct DistributionEntry { std::string target, source; std::vector<FormKey> tracks; };
    std::vector<DistributionEntry> distributionEntries_;
    std::vector<std::string> configErrors_;
    struct Definition { FormKey key; std::string source,editorID,type; bool deleted{}; };
    std::vector<Definition> definitions_;
    struct RawFilter { SelectorKind kind; bool required{}; std::vector<std::string> forms; };
    struct RawSelector { bool combat{}; std::string music,source; std::vector<RawFilter> filters; };
    std::vector<RawSelector> selectorInputs_;
};
std::string ToString(const FormKey& key);
}
