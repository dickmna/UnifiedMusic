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
};
struct MergePlan {
    std::map<FormKey, std::vector<FormKey>> additions;
    std::set<FormKey> allowed;
    std::set<FormKey> scriptBound;
    std::optional<FormKey> battleType;
    std::vector<AutoCombatRule> combatRules;
    std::vector<std::string> diagnostics;
};
class PluginScan {
public:
    void Read(const std::filesystem::path& path);
    MergePlan Plan() const;
    const auto& Records() const { return music_; }
private:
    void InferCombat(MergePlan& plan) const;
    std::map<FormKey, std::vector<MusicRecord>> music_;
    std::map<FormKey, std::vector<TrackRecord>> tracks_;
    std::map<FormKey, AreaKind> areaForms_;
    std::set<FormKey> deletedAreas_, mappedTypes_;
    std::vector<ScriptProperties> scripts_;
};
std::string ToString(const FormKey& key);
}
