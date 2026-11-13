#include "PCH.h"
#include "Module.h"
// Adapted from SeaSparrowOG/CombatMusic, AGPL-3.0; see vendor/CME/LICENSE.
#include "hooks/hooks.h"

namespace CME::Hooks {
    namespace {
        bool IsActive(const RE::BGSMusicType* music)
        {
            if (!music) return false;
            // MUSIC_STATUS is an ordinal enum, not a bit-mask (Finishing = 3).
            const auto state = music->typeStatus.underlying();
            using Status = RE::BSIMusicType::MUSIC_STATUS;
            return state == static_cast<std::uint32_t>(Status::kPlaying) ||
                   state == static_cast<std::uint32_t>(Status::kPaused) ||
                   state == static_cast<std::uint32_t>(Status::kFinishing);
        }
    }
	bool Install()
	{
		return CombatMusicCalls::GetSingleton()->Install();
	}

	bool CombatMusicCalls::Install()
	{
        static bool installed = false;
        if (installed) return true;
        const auto version = REX::FModule::GetExecutingModule().GetFileVersion();
        if (version != REL::Version(1, 7, 104, 0) && version != REL::Version(1, 7, 99, 0)) {
            REX::ERROR("CME: refusing unverified runtime {}", version.string());
            return false;
        }
        // These are the Steam 1.7 call sites used by the pinned upstream.
        const std::array<std::uintptr_t, 6> sites{
            REL::Relocation<std::uintptr_t>{ REL::ID(46897), 0x20 }.address(),
            REL::Relocation<std::uintptr_t>{ REL::ID(46870), 0x1C1 }.address(),
            REL::Relocation<std::uintptr_t>{ REL::ID(46870), 0x22D }.address(),
            REL::Relocation<std::uintptr_t>{ REL::ID(18369), 0x173 }.address(),
            REL::Relocation<std::uintptr_t>{ REL::ID(51653), 0x29C }.address(),
            REL::Relocation<std::uintptr_t>{ REL::ID(46895), 0x3B4 }.address()
        };
        for (const auto site : sites) {
            if (*reinterpret_cast<const std::uint8_t*>(site) != 0xE8) {
                REX::ERROR("CME: call-site validation failed at {:X}; no hooks installed", site);
                return false;
            }
        }
        auto& trampoline = REL::GetTrampoline();
        _revertCombatMusic = trampoline.write_call<5>(sites[0], &RevertCombatMusic);
        _startCombatMusic = trampoline.write_call<5>(sites[1], &StartCombatMusic);
        _endCombatMusic = trampoline.write_call<5>(sites[2], &EndCombatMusic);
        _clearLocation = trampoline.write_call<5>(sites[3], &ClearLocation);
        _discoveryMusic = trampoline.write_call<5>(sites[4], &DiscoveryMusic);
        _loadCombatMusic = trampoline.write_call<5>(sites[5], &LoadCombatMusic);
        installed = true;

		REX::INFO("Finished installing hooks.");
		REX::INFO("___________________________________________________");
		return true;
	}

	RE::BGSMusicType* CombatMusicCalls::GetCurrentCombatMusic()
	{
		return storedMusic;
	}

	void CombatMusicCalls::SetCurrentCombatMusic(RE::BGSMusicType* a_combatMusic)
	{
		storedMusic = a_combatMusic;
	}

	void CombatMusicCalls::PushNewCombatMusic(ConditionalBattleMusic&& newMusic)
	{
		if (newMusic.conditions.empty()) {
			return;
		}
		conditionalMusic.push_back(std::move(newMusic));
	}

	void CombatMusicCalls::PushNewClearedMusic(ConditionalBattleMusic&& newMusic)
	{
		if (newMusic.conditions.empty()) {
			return;
		}
		conditionalClearedMusic.push_back(std::move(newMusic));
	}

	RE::BGSMusicType* CombatMusicCalls::GetAppropriateCombatMusic(RE::BGSMusicType* a_music)
	{
		if (!a_music) {
			return a_music;
		}

		const auto defaultObjects = RE::BGSDefaultObjectManager::GetSingleton();
		assert(defaultObjects);
		const auto MUSCombat = defaultObjects->GetObject<RE::BGSMusicType>(
			RE::BGSDefaultObjectManager::DefaultObject::kBattleMusic);
		if (!MUSCombat || a_music != MUSCombat) {
			return a_music;
		}

        // Repeated battle queries must not abandon ownership of a still-active
        // replacement. Retargeting would leave the previous queued type behind.
        if (IsActive(storedMusic)) {
            return CME::AllowsReplacement(storedMusic, true) ? storedMusic : a_music;
        }
		storedMusic = nullptr;
		RE::BGSMusicType* newMusic = nullptr;
		int bestMatch = 0;
		PriorityLevel bestPriorityLevel = PriorityLevel::LOW;
		for (const auto& candidate : conditionalMusic) {
			// A script can obtain MusicType dynamically without a VMAD property.
			// Never claim an already-active type that this module did not start.
			if (IsActive(candidate.music)) continue;
			if (!CME::AllowsReplacement(candidate.music, true)) continue;
			const auto candidateMatch = candidate.MatchDegree();
			if (candidateMatch.first == PriorityLevel::HIGH && bestPriorityLevel == PriorityLevel::LOW) {
				bestPriorityLevel = PriorityLevel::HIGH;
				bestMatch = candidateMatch.second;
				newMusic = candidate.music;
			}
			else if (candidateMatch.first == PriorityLevel::LOW && bestPriorityLevel == PriorityLevel::HIGH) {
				continue;
			}
			else if (bestMatch < candidateMatch.second) {
				bestMatch = candidateMatch.second;
				newMusic = candidate.music;
			}
		}

		if (newMusic) {
			REX::DEBUG("  Starting {}", Utilities::EDID::GetEditorID(newMusic));
			storedMusic = newMusic;
			return newMusic;
		}
		return a_music;
	}

	RE::BGSMusicType* CombatMusicCalls::GetAppropriateClearedMusic(RE::BGSMusicType* a_music)
	{
		RE::BGSMusicType* newMusic = nullptr;
		int bestMatch = 0;
		PriorityLevel bestPriorityLevel = PriorityLevel::LOW;
		for (const auto& candidate : conditionalClearedMusic) {
			if (IsActive(candidate.music)) continue;
			if (!CME::AllowsReplacement(candidate.music, false)) continue;
			const auto candidateMatch = candidate.MatchDegree();
			if (candidateMatch.first == PriorityLevel::HIGH && bestPriorityLevel == PriorityLevel::LOW) {
				bestPriorityLevel = PriorityLevel::HIGH;
				bestMatch = candidateMatch.second;
				newMusic = candidate.music;
			}
			else if (candidateMatch.first == PriorityLevel::LOW && bestPriorityLevel == PriorityLevel::HIGH) {
				continue;
			}
			else if (bestMatch < candidateMatch.second) {
				bestMatch = candidateMatch.second;
				newMusic = candidate.music;
			}
		}

		if (newMusic) {
			REX::DEBUG("  Starting {}", Utilities::EDID::GetEditorID(newMusic));
			return newMusic;
		}
		return a_music;
	}

	RE::BGSMusicType* CombatMusicCalls::ClearMusic()
	{
		const auto callsSingleton = CombatMusicCalls::GetSingleton();
		const auto musicToStop = callsSingleton->storedMusic;
		callsSingleton->storedMusic = nullptr;

		if (!musicToStop) {
			const auto defaultObjects = RE::BGSDefaultObjectManager::GetSingleton();
			assert(defaultObjects);
			const auto MUSCombat = defaultObjects->GetObject<RE::BGSMusicType>(
				RE::BGSDefaultObjectManager::DefaultObject::kBattleMusic);
			assert(MUSCombat);

			return MUSCombat;
		}
		REX::DEBUG("  Stopping {}", Utilities::EDID::GetEditorID(musicToStop));
		return musicToStop;
	}

	RE::BGSMusicType* CombatMusicCalls::RevertCombatMusic(RE::DEFAULT_OBJECT a1)
	{
#ifdef DEBUG
		const auto obj = RE::BGSDefaultObjectManager::GetSingleton()->GetObject(a1);
		REX::DEBUG("Revert combat music hook ({})", obj ? Utilities::EDID::GetEditorID(obj) : "NULL");
#endif
		const auto response = _revertCombatMusic(a1);
		if (a1 != RE::BGSDefaultObjectManager::DefaultObject::kBattleMusic) {
			return response;
		}

		return CombatMusicCalls::GetSingleton()->ClearMusic();
	}

	RE::BGSMusicType* CombatMusicCalls::StartCombatMusic(RE::DEFAULT_OBJECT a1)
	{
#ifdef DEBUG
		const auto obj = RE::BGSDefaultObjectManager::GetSingleton()->GetObject(a1);
		REX::DEBUG("Start combat music hook ({})", obj ? Utilities::EDID::GetEditorID(obj) : "NULL");
#endif
		const auto response = _startCombatMusic(a1);
		if (a1 != RE::BGSDefaultObjectManager::DefaultObject::kBattleMusic) {
			return response;
		}

		return CombatMusicCalls::GetSingleton()->GetAppropriateCombatMusic(response);
	}

	RE::BGSMusicType* CombatMusicCalls::LoadCombatMusic(RE::DEFAULT_OBJECT a1)
	{
#ifdef DEBUG
		const auto obj = RE::BGSDefaultObjectManager::GetSingleton()->GetObject(a1);
		REX::DEBUG("Load combat music hook ({})", obj ? Utilities::EDID::GetEditorID(obj) : "NULL");
#endif
		const auto response = _loadCombatMusic(a1);
		if (a1 != RE::BGSDefaultObjectManager::DefaultObject::kBattleMusic) {
			return response;
		}

		return CombatMusicCalls::GetSingleton()->GetAppropriateCombatMusic(response);
	}

	RE::BGSMusicType* CombatMusicCalls::EndCombatMusic(RE::DEFAULT_OBJECT a1)
	{
#ifdef DEBUG
		const auto obj = RE::BGSDefaultObjectManager::GetSingleton()->GetObject(a1);
		REX::DEBUG("End combat music hook ({})", obj ? Utilities::EDID::GetEditorID(obj) : "NULL");
#endif
		const auto response = _endCombatMusic(a1);
		if (a1 != RE::BGSDefaultObjectManager::DefaultObject::kBattleMusic) {
			return response;
		}

		return CombatMusicCalls::GetSingleton()->ClearMusic();
	}

	RE::BGSMusicType* CombatMusicCalls::DiscoveryMusic(RE::DEFAULT_OBJECT a1)
	{
#ifdef DEBUG
		const auto obj = RE::BGSDefaultObjectManager::GetSingleton()->GetObject(a1);
		REX::DEBUG("Discovery music hook ({})", obj ? Utilities::EDID::GetEditorID(obj) : "NULL");
#endif
		if (a1 == RE::BGSDefaultObjectManager::DefaultObject::kBattleMusic) {
			const auto storedMusic = GetSingleton()->storedMusic;
			if (storedMusic) {
				return storedMusic;
			}
		}
		return _discoveryMusic(a1);
	}

	RE::BGSMusicType* CombatMusicCalls::ClearLocation(RE::DEFAULT_OBJECT a1)
	{
#ifdef DEBUG
		const auto obj = RE::BGSDefaultObjectManager::GetSingleton()->GetObject(a1);
		REX::DEBUG("Clear location music hook ({})", obj ? Utilities::EDID::GetEditorID(obj) : "NULL");
#endif
		const auto response = _clearLocation(a1);
		if (a1 != RE::BGSDefaultObjectManager::DefaultObject::kDungeonClearedMusic) {
			return response;
		}

		return CombatMusicCalls::GetSingleton()->GetAppropriateClearedMusic(response);
	}
}
