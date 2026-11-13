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
};
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
};
struct ConditionTerm {
    std::uint32_t function{}, comparison{}, parameter2{};
    FormKey parameter;
    std::uint8_t opcode{};
    bool onPlayerReference{};
    auto operator<=>(const ConditionTerm&) const = default;
};
struct ConditionVersion {
    std::string source;
    std::vector<ConditionTerm> conditions;
};
struct ConditionRepair {
    FormKey track;
    std::vector<Area> exclusions;
    std::vector<ConditionVersion> versions;
    std::vector<FormKey> requiredBattleTracks;
    bool genericGuard{};
};
struct ConditionWitness {
    FormKey track;
    std::vector<ConditionVersion> versions;
};
struct MergePlan {
    std::map<FormKey, std::vector<FormKey>> additions;
    std::set<FormKey> allowed;
    std::set<FormKey> scriptBound;
    std::optional<FormKey> battleType;
    std::vector<AutoCombatRule> combatRules;
    bool battleMergeSafe{};
    std::vector<ConditionRepair> conditionRepairs;
    std::vector<ConditionWitness> conditionWitnesses;
    std::vector<MappingRepair> mappingRepairs;
    std::vector<std::string> diagnostics;
};
class PluginScan {
public:
    void Read(const std::filesystem::path& path);
    MergePlan Plan() const;
    const auto& Records() const { return music_; }
private:
    bool ScriptReferences(const FormKey& key) const;
    bool DependsOn(const std::string& source, const std::string& dependency) const;
    std::set<FormKey> ProtectedTracks(const FormKey& excludedType, const std::set<FormKey>& scriptBound) const;
    void InferCombat(MergePlan& plan) const;
    void InferConditionRepairs(MergePlan& plan) const;
    void InferMappings(MergePlan& plan) const;
    std::map<FormKey, std::vector<MusicRecord>> music_;
    std::map<FormKey, std::vector<TrackRecord>> tracks_;
    std::map<FormKey, AreaKind> areaForms_;
    std::map<FormKey, std::vector<MappingRecord>> mappings_;
    std::set<FormKey> deletedAreas_, mappedTypes_, nestedTracks_;
    std::vector<ScriptProperties> scripts_;
    std::map<std::string, std::vector<std::string>> dependencies_;
};
std::string ToString(const FormKey& key);
}
