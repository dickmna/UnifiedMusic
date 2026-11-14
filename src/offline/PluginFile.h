#pragma once
#include "core/PluginRecords.h"
#include <filesystem>

namespace UnifiedMusic {
class PluginScan;
// Portable transport for the inspector/tests only. Never linked into the DLL.
void ReadPluginRecords(const std::filesystem::path& path, const RecordConsumer& consume);
void ReadPluginFile(PluginScan& scan, const std::filesystem::path& path);
}
