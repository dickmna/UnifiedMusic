#include "AreaMusic.h"
#include "core/PluginScan.h"
#include <RE/Skyrim.h>
#include <REX/REX.h>
#include <memory>

namespace UnifiedMusic {
void RestoreAreaMusic(const MergePlan& plan) {
    auto* data=RE::TESDataHandler::GetSingleton();
    if(!data) return;
    std::size_t restored=0;
    for(const auto& repair:plan.mappingRepairs) {
        const auto skip=[&](const char* reason){REX::WARN("Preserve area {}: {}",ToString(repair.area.form),reason);};
        auto* form=data->LookupForm<RE::TESForm>(repair.area.form.id,repair.area.form.plugin);
        auto* desired=data->LookupForm<RE::BGSMusicType>(repair.music.id,repair.music.plugin);
        if(!form || !desired || !form->GetFile()) {skip("missing native form");continue;}
        std::string source(form->GetFile()->GetFilename());
        for(auto& c:source) if(c>='A' && c<='Z') c+=32;
        const MappingRecord* snapshot=nullptr;
        for(const auto& version:repair.versions) if(version.source==source) snapshot=&version;
        if(!snapshot) {skip("unrecognized winning source");continue;}
        RE::BGSMusicType* expected=nullptr;
        if(snapshot->music) {
            expected=data->LookupForm<RE::BGSMusicType>(snapshot->music->id,snapshot->music->plugin);
            if(!expected) {skip("unresolved scanned assignment");continue;}
        }
        RE::BGSMusicType** slot=nullptr;
        RE::TESObjectCELL* cell=nullptr;
        switch(repair.area.kind) {
        case AreaKind::Worldspace:
            if(auto* world=form->As<RE::TESWorldSpace>()) slot=&world->musicType;
            break;
        case AreaKind::Location:
            if(auto* location=form->As<RE::BGSLocation>()) slot=&location->musicType;
            break;
        case AreaKind::Cell:
            cell=form->As<RE::TESObjectCELL>();
            if(cell) if(auto* extra=cell->extraList.GetByType<RE::ExtraCellMusicType>()) slot=&extra->type;
            break;
        default: break;
        }
        if(!slot && !cell) {skip("unsupported native area type");continue;}
        auto* current=slot?*slot:nullptr;
        if(current==desired) continue;
        if(current!=expected) {skip("assignment changed since plugin loading");continue;}
        if(slot) *slot=desired;
        else {
            // XCMO can be absent in the winning CELL. Add only its music extra;
            // lighting, water, ownership and all other cell data stay native.
            std::unique_ptr<RE::ExtraCellMusicType> extra(RE::BSExtraData::Create<RE::ExtraCellMusicType>());
            extra->type=desired;
            cell->extraList.Add(extra.get());
            if(cell->extraList.GetByType<RE::ExtraCellMusicType>()!=extra.get()) {skip("cell music extra was not installed");continue;}
            extra.release();
        }
        ++restored;
        REX::INFO("Restore area {} -> {}",ToString(repair.area.form),ToString(repair.music));
    }
    REX::INFO("Area music: {} assignments restored",restored);
}
}
