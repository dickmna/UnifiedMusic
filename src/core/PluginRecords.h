#pragma once
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace UnifiedMusic {
using PluginBytes = std::span<const std::uint8_t>;
using PluginFields = std::vector<std::pair<std::string, PluginBytes>>;
// Borrowed for one consumer call. IDs in both headers and fields use the
// source file's master indices, never the running game's compile indices.
struct PluginRecord {
    std::string type;
    std::uint32_t flags{}, id{};
    PluginFields fields;
};
using RecordConsumer = std::function<void(const PluginRecord&)>;
using ReadRecords = std::function<void(const RecordConsumer&)>;

inline bool PluginRecordNeedsID(std::string_view type) {
    return type=="MUSC" || type=="MUST" || type=="WRLD" || type=="CELL" ||
        type=="LCTN" || type=="REGN" || type=="KYWD" || type=="NPC_" ||
        type=="QUST" || type=="GLOB";
}
inline bool NeedsPluginField(std::string_view type, std::string_view field) {
    if(type=="MUSC" || type=="MUST") return true; // Settings/metadata compare every field.
    if(type=="TES4") return field=="MAST";
    if(field=="VMAD") return true; // Script owners are not limited to music records.
    if(field=="EDID") return PluginRecordNeedsID(type);
    return (type=="CELL" && field=="XCMO") || (type=="WRLD" && field=="ZNAM") ||
        (type=="LCTN" && (field=="NAM1" || field=="PNAM")) || (type=="REGN" && field=="RDMO");
}
}
