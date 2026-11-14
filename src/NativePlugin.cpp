#include "NativePlugin.h"
#include "NativePluginInput.h"
#include "core/PluginScan.h"
#include <RE/Skyrim.h>

namespace UnifiedMusic {
void ReadNativePlugin(PluginScan& scan,RE::TESFile& file) {
    const std::string name(file.GetFilename());
    try {
        scan.Read(name,[&](const RecordConsumer& consume) {
            NativeInput::Read(file,
                [](auto& input) {return input.OpenTES(RE::NiFile::OpenMode::kReadOnly,false);},
                [](auto& input) {
                    // Steam 1.7.104 has a third argument missing from the pinned
                    // SDK declaration. Do not skip ignored/deleted history;
                    // defer subrecord initialization until after header checks.
                    using Next=bool(RE::TESFile*,bool,bool);
                    static REL::Relocation<Next> next{RELOCATION_ID(13894,13979)};
                    return next(&input,false,true);
                },consume);
        });
    } catch(const std::exception& error) {
        throw std::runtime_error("native plugin "+name+": "+error.what());
    }
}
}
