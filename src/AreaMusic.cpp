#include "AreaMusic.h"
#include "core/PluginScan.h"
#include <RE/Skyrim.h>
#include <REX/REX.h>
#include <memory>

namespace UnifiedMusic {
void RestoreAreaMusic(const MergePlan& plan,const std::set<FormKey>& failed,bool automaticComplete) {
    auto* data=RE::TESDataHandler::GetSingleton();
    if(!data) return;
    for(const auto& [area,music]:plan.assignments) {
        if(failed.contains(music)) {REX::WARN("Configured area {} deferred: target playlist failed",ToString(area.form));continue;}
        auto* form=data->LookupForm<RE::TESForm>(area.form.id,area.form.plugin);
        auto* desired=data->LookupForm<RE::BGSMusicType>(music.id,music.plugin);
        if(!form || !desired) {REX::WARN("Configured area {} missing native form",ToString(area.form));continue;}
        std::size_t assigned=0;
        // Same native operations as MTD Distributor::Distribute. Region entry
        // flags/priority, sound effects, weather and geometry remain untouched.
        if(area.kind==AreaKind::Location) {
            if(auto* location=form->As<RE::BGSLocation>()) {location->musicType=desired;++assigned;}
        } else if(area.kind==AreaKind::Region) {
            if(auto* region=form->As<RE::TESRegion>();region && region->dataList && data->regionDataManager)
                for(auto* entry:region->dataList->regionDataList)
                    if(entry) if(auto* sound=data->regionDataManager->AsRegionDataSound(entry)) {sound->music=desired;++assigned;}
        }
        REX::INFO("Configured area {} -> {}: {} music slots assigned",ToString(area.form),ToString(music),assigned);
    }
    // An unrelated automatic failure must not cancel valid explicit mappings.
    if(!automaticComplete) {REX::WARN("Automatic area recovery deferred after a music preparation failure");return;}
    std::size_t restored=0;
    for(const auto& repair:plan.mappingRepairs) {
        if(plan.assignments.contains(repair.area)) continue;
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
