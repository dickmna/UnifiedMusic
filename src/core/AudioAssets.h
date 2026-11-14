#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "PluginScan.h"

namespace UnifiedMusic {
using AudioBytes=std::vector<std::uint8_t>;
inline constexpr std::size_t MaxAudioBytes=64*1024*1024;
std::optional<std::string> MusicPath(std::string path);
bool ValidMusic(std::span<const std::uint8_t> bytes);
std::string AudioHash(std::span<const std::uint8_t> bytes);
std::optional<AudioBytes> ReadAudioFile(const std::filesystem::path& path);
using AudioReader=std::function<std::optional<AudioBytes>(const std::string&)>;
using AudioPaths=std::pair<std::string,std::string>;
struct AudioPair { std::string track,finale,trackHash,finaleHash,replacementHash,replacementFinaleHash; };
inline constexpr auto AudioManifest="SKSE/Plugins/HarmoniaOriginalAudio.json";
// Optional, independently installed resources. No archive parser or disk writes.
class OriginalAudioPack {
public:
    void Read(const std::filesystem::path& manifest);
    bool Empty() const { return assets_.empty(); }
    std::optional<AudioPair> Resolve(const AudioPaths& source,const std::vector<AudioPaths>& present,
        const AudioReader& reader,std::string& reason);
private:
    struct Asset { std::string file,hash; std::set<std::string> equivalent; };
    std::map<std::string,Asset> assets_;
    std::set<AudioPaths> pairs_;
    // Startup-only fingerprints: audio bytes are released immediately.
    std::map<std::string,std::string> fingerprints_;
    std::string Fingerprint(const std::string& path,const AudioReader& reader);
};
std::map<FormKey,std::string> CheckSceneResources(const std::vector<SceneResource>& resources,const AudioReader& reader);
}
