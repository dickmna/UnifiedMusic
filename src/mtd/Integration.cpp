// MTD-derived INI distribution, GPL-3.0-or-later with vendor/MTD/EXCEPTIONS.
// Original: ThirdEyeSqueegee, MusicTypeDistributor 1.4.0.
#include "Integration.h"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <RE/Skyrim.h>
#include <REX/REX.h>
#include <SimpleIni.h>

namespace MTD {
namespace {
TypePolicy typePolicy{};
bool prepared{}, applied{};
std::unordered_map<RE::BGSMusicType*, std::vector<RE::BGSMusicTrackFormWrapper*>> pending;

std::string Lower(std::string value)
{
    std::ranges::transform(value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
std::string_view Trim(std::string_view value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n")-first+1);
}
std::string Name(const RE::BGSMusicType* type)
{
    const auto* value = type ? type->GetFormEditorID() : nullptr;
    return value ? value : "";
}
bool DefaultAmbientPolicy(const RE::BGSMusicType* type)
{
    if (!type) return false;
    const auto* origin = type->GetFile(0);
    if (!origin) return false;
    const auto file = Lower(std::string{origin->GetFilename()});
    // These are engine base-file identities, never a third-party mod list.
    if (file != "skyrim.esm" && file != "dawnguard.esm" && file != "dragonborn.esm" && file != "hearthfires.esm") return false;
    const auto name = Lower(Name(type));
    for (const auto* forbidden : {"combat", "boss", "silent", "silence", "blank", "stinger", "reward", "reveal", "quest", "test"}) {
        if (name.find(forbidden) != std::string::npos) return false;
    }
    const auto pos = name.find("mus");
    if (pos == std::string::npos) return false;
    const std::string_view purpose{name.data()+pos+3, name.size()-pos-3};
    return purpose.starts_with("explore") || purpose.starts_with("town") ||
           purpose.starts_with("dungeon") || purpose.starts_with("tavern");
}
RE::BGSMusicType* FindType(std::string_view editorID)
{
    // MusicType retains EDID; no po3 Tweaks cache or static EditorID lookup required.
    auto* handler = RE::TESDataHandler::GetSingleton();
    for (auto* type : handler->GetFormArray<RE::BGSMusicType>()) {
        if (type && Name(type) == editorID) return type;
    }
    return nullptr;
}
void AddTrackTokens(RE::BGSMusicType* type, std::string_view csv)
{
    while (!csv.empty()) {
        const auto comma = csv.find(',');
        const auto token = Trim(csv.substr(0, comma));
        csv = comma == std::string_view::npos ? std::string_view{} : csv.substr(comma+1);
        const auto tilde = token.find('~');
        if (tilde == std::string_view::npos) {
            REX::WARN("MTD: invalid track identifier {}", token);
            continue;
        }
        auto hex = Trim(token.substr(0, tilde));
        const auto plugin = Trim(token.substr(tilde+1));
        if (hex.starts_with("0x") || hex.starts_with("0X")) hex.remove_prefix(2);
        if (hex.empty() || plugin.empty()) {
            REX::WARN("MTD: invalid track identifier {}", token);
            continue;
        }
        RE::FormID id{};
        const auto [end, error] = std::from_chars(hex.data(), hex.data()+hex.size(), id, 16);
        if (hex.empty() || plugin.empty() || !id || error != std::errc{} || end != hex.data()+hex.size()) {
            REX::WARN("MTD: invalid track identifier {}", token);
            continue;
        }
        auto* track = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSMusicTrackFormWrapper>(id, plugin);
        if (!track) {
            REX::WARN("MTD: unresolved track {}", token);
            continue;
        }
        auto& target = pending[type];
        if (std::ranges::find(target, track) == target.end()) target.push_back(track);
    }
}
}

void SetTypePolicy(TypePolicy policy) noexcept { typePolicy = policy; }
bool CanAppendTo(const RE::BGSMusicType* type) noexcept
{
    return type && (typePolicy ? typePolicy(type) : DefaultAmbientPolicy(type));
}

static bool PrepareDistributionImpl()
{
    if (prepared || applied) return prepared;
    if (!RE::TESDataHandler::GetSingleton()) return false;
    std::error_code error;
    std::set<std::filesystem::path> configs;
    const std::filesystem::directory_iterator first{"Data", error};
    if (error) {
        REX::ERROR("MTD: cannot read Data directory: {}", error.message());
        return false;
    }
    for (const auto& file : first) {
        if (file.is_regular_file(error) && Lower(file.path().filename().string()).ends_with("_mus.ini")) configs.insert(file.path());
    }
    for (const auto& path : configs) {
        CSimpleIniA ini{true, true, false};
        if (ini.LoadFile(path.c_str()) < 0) {
            REX::WARN("MTD: cannot read {}", path.filename().string());
            continue;
        }
        if (ini.SectionExists("Location") || ini.SectionExists("Region")) {
            REX::WARN("MTD: preserving authored locations/regions; redirect sections skipped in {}", path.filename().string());
        }
        CSimpleIniA::TNamesDepend keys;
        ini.GetAllKeys("General", keys);
        for (const auto& key : keys) {
            auto editorID = Trim(key.pItem);
            if (editorID.ends_with('!')) {
                editorID.remove_suffix(1);
                editorID = Trim(editorID);
                REX::WARN("MTD: ignoring clear request for {}; append only", editorID);
            }
            if (editorID.empty()) continue;
            auto* type = FindType(editorID);
            if (!type) {
                REX::WARN("MTD: unknown target {}; no runtime MusicType creation", editorID);
                continue;
            }
            CSimpleIniA::TNamesDepend values;
            ini.GetAllValues("General", key.pItem, values);
            for (const auto& value : values) AddTrackTokens(type, value.pItem);
        }
    }
    prepared = true;
    return true;
}

bool PrepareDistribution() noexcept
{
    try {
        return PrepareDistributionImpl();
    } catch (const std::exception& error) {
        pending.clear();
        REX::ERROR("MTD: configuration distribution skipped: {}", error.what());
        return false;
    }
}

void ApplyDistribution() noexcept
{
    if (!prepared || applied) return;
    for (const auto& [type, tracks] : pending) {
        if (!CanAppendTo(type)) {
            REX::WARN("MTD: preserving protected music type {} (0x{:x})", Name(type), type->GetFormID());
            continue;
        }
        for (auto* track : tracks) {
            if (std::ranges::find(type->tracks, track) != type->tracks.end()) continue;
            // Reuse the original track form: conditions and playback settings survive.
            type->tracks.emplace_back(track);
            REX::INFO("MTD: appended track 0x{:x} to {}", track->GetFormID(), Name(type));
        }
    }
    pending.clear();
    applied = true;
}

bool RunDistribution() noexcept
{
    if (!PrepareDistribution()) return false;
    ApplyDistribution();
    return true;
}
}
