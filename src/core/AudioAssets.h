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
// Original archive access is deliberately separate from the winning-resource
// reader. The game reader supplies deployed/VFS/BSA precedence at runtime.
class OriginalAudio {
public:
    void AddArchive(const std::filesystem::path& path);
    std::optional<AudioBytes> Read(const std::string& path) const;
private:
    struct Entry { std::filesystem::path archive; std::uint32_t offset{},size{}; bool compressed{},embedded{}; };
    std::map<std::string,Entry> entries_;
    std::vector<std::string> ambiguous_;
};
using AudioReader=std::function<std::optional<AudioBytes>(const std::string&)>;
struct AudioPair { AudioBytes track,finale; std::string trackHash,finaleHash,replacementHash,replacementFinaleHash; };
// No writes. Returns only complete, valid, byte-distinct original/replacer pairs.
std::optional<AudioPair> CompareAudio(const OriginalAudio& original,const AudioReader& visible,
    const std::string& track,const std::string& finale,std::string& reason);
std::string CacheAudio(const std::filesystem::path& dataRoot,const AudioBytes& bytes);
std::map<FormKey,std::string> CheckSceneResources(const std::vector<SceneResource>& resources,const AudioReader& reader);
}
