#pragma once
#include <fstream>
#include <iostream>
#include <filesystem>
#include <unordered_set>
#include <moar_ptr.h>
#include <API/ARK/Ark.h>
#include <Timer.h>
#include "json.hpp"

namespace ArkAutoBosses
{
	using json = nlohmann::json;

	static auto ReadJSON(std::filesystem::path&& path) -> json
	{
		const auto error = fmt::format("Unable to read file {}", std::filesystem::absolute(path).string());
		json read;
		if (!std::filesystem::exists(path))
			throw std::runtime_error(error);

		std::ifstream file{ std::filesystem::absolute(path).string() };
		if (!file.is_open())
			throw std::runtime_error(error);

		file >> read;
		return read;
	}

	class DefeatedBoss
	{
	public:
		std::string Boss;
		std::string Blueprint;
		std::uint32_t Difficulty;
		bool operator==(const DefeatedBoss& other) const
		{
			return this->Difficulty == other.Difficulty && this->Boss == other.Boss && this->Blueprint == other.Blueprint;
		}
		struct Hash
		{
			[[nodiscard]] auto operator()(ArkAutoBosses::DefeatedBoss const& a) const noexcept { return std::hash<std::string>{}(a.Boss + a.Blueprint + std::to_string(a.Difficulty)); }
		};
	};

	class Config
	{
	public:
		std::int32_t chibiLevels = 0;
		std::unordered_set<DefeatedBoss, DefeatedBoss::Hash> defeatedBosses = {};
		std::unordered_set<std::string> additionalEngrams = {};
		bool unlockExplorerNotes = false;
		std::unordered_set<std::string> generalizedAchievementTagGrants = {};
	};

	static void from_json(const json& json, DefeatedBoss& defeatedBoss)
	{
		json.at("Boss").get_to(defeatedBoss.Boss);
		json.at("Blueprint").get_to(defeatedBoss.Blueprint);
		json.at("Difficulty").get_to(defeatedBoss.Difficulty);
	}

	static void from_json(const json& json, Config& config)
	{
		json.at("ChibiLevels").get_to(config.chibiLevels);
		json.at("DefeatedBosses").get_to(config.defeatedBosses);
		json.at("AdditionalEngrams").get_to(config.additionalEngrams);
		json.at("UnlockExplorerNotes").get_to(config.unlockExplorerNotes);
		json.at("GeneralizedAchievementTagGrants").get_to(config.generalizedAchievementTagGrants);
	}

	class Plugin
	{
		static inline moar::function_ptr<void(AShooterGameMode*, APlayerController*, bool, bool, const FPrimalPlayerCharacterConfigStruct&, UPrimalPlayerData*)> StartNewShooterPlayer;

		static inline Config config;

		static inline std::unordered_set<UClass*> unlocksCache;

		static inline UObject* PrimalPlayerDataBPBaseCDO;

		static auto CacheRuntimeData() -> void
		{
			for (auto&& boss : config.defeatedBosses)
			{
				FString bossText = FString(boss.Blueprint.c_str());
				UClass* bossClass = UVictoryCore::BPLoadClass(&bossText);


				if (!bossClass)
					continue;

				APrimalDinoCharacter* bossCharacter = static_cast<APrimalDinoCharacter*>(bossClass->GetDefaultObject(true));

				if (!bossCharacter)
					continue; //unable to load this class, skip.

				for (auto&& engram : bossCharacter->DeathGiveEngramClassesField())
					unlocksCache.emplace(engram.uClass);
			}
			for (auto&& engram : config.additionalEngrams)
			{
				FString engramText = FString(engram.c_str());
				UClass* engramClass = UVictoryCore::BPLoadClass(&engramText);

				if (!engramClass)
					continue;

				unlocksCache.emplace(engramClass);
			}
			PrimalPlayerDataBPBaseCDO = UVictoryCore::BPLoadClass("Blueprint'/Game/PrimalEarth/CoreBlueprints/PrimalPlayerDataBP_Base.PrimalPlayerDataBP_Base'")->GetDefaultObject(true);
			Log::GetLog()->info("Cached {} total unlocks.", unlocksCache.size());
			Log::GetLog()->info("Cached PrimalPlayerDataBPBaseCDO. ({})", (void*) PrimalPlayerDataBPBaseCDO);
		}

		static auto ProcessEngrams(AShooterPlayerController* playerController, AShooterPlayerState* playerState) -> void
		{
			for (auto&& engram : unlocksCache)
			{
				playerState->ServerUnlockEngram(engram, false, true);
			}
		}

		static auto ProcessBosses(AShooterPlayerController* playerController, UPrimalPlayerData* playerData) -> void
		{
			struct UFunctionParameters
			{
				AShooterCharacter* BossCharacter;
				uint32_t DifficultyIndex;
				FName TagOverride;
				AShooterPlayerController* Controller;
			};
			static UFunction* defeatedBossFunction = PrimalPlayerDataBPBaseCDO->FindFunctionChecked(FName("DefeatedBoss", EFindName::FNAME_Find));
			
			for (auto&& boss : config.defeatedBosses)
			{
				UFunctionParameters parameters = { nullptr, boss.Difficulty, FName(boss.Boss.c_str(), EFindName::FNAME_Find), playerController };
				playerData->ProcessEvent(defeatedBossFunction, &parameters);
			}
		}

		static auto ProcessChibi(AShooterPlayerController* playerController, UPrimalPlayerData* playerData) -> void
		{
			static UProperty* playerChibiLevels = PrimalPlayerDataBPBaseCDO->FindProperty(FName("NumChibiLevelUpsData", EFindName::FNAME_Find));

			if (!playerChibiLevels)
				return;

			playerChibiLevels->Set(playerData, config.chibiLevels);
			playerData->SetChibiLevels(config.chibiLevels, playerController);
		}

		static auto ProcessAchievementTagGrants(AShooterPlayerController* playerController, UPrimalPlayerData* playerData)
		{
			for (auto&& grant : config.generalizedAchievementTagGrants)
				playerData->GrantGeneralizedAchievementTag(FName(grant.c_str(), EFindName::FNAME_Find), playerController);
		}

		static auto ProcessExplorerNotes(AShooterPlayerController* playerController, UPrimalPlayerData* playerData)
		{
			AShooterCharacter* playerCharacter = playerController->LastControlledPlayerCharacterField().Get();

			if (!config.unlockExplorerNotes || !playerData->MyDataField() || !playerData->MyDataField()->MyPersistentCharacterStatsField())
				return;

			playerData->MyDataField()->MyPersistentCharacterStatsField()->bHasUnlockedAllExplorerNotes() = true;

			if (playerCharacter)
			{
				playerCharacter->BPUnlockedAllExplorerNotes();
				playerCharacter->ForceReplicateNow(false, false);
			}
		}

		static auto ProcessUnlocks(AShooterPlayerController* playerController) -> void
		{
			AShooterPlayerState* playerState;
			UPrimalPlayerData* playerData;
			
			if ( /* Guard */
				!(playerState = playerController->GetShooterPlayerState()) || 
				!(playerData = playerState->MyPlayerDataField())
				)
				return;
			/* End Guard */

			ProcessEngrams(playerController, playerState);
			ProcessBosses(playerController, playerData);
			ProcessChibi(playerController, playerData);
			ProcessAchievementTagGrants(playerController, playerData);
			ProcessExplorerNotes(playerController, playerData);
			
			playerData->SavePlayerData(ArkApi::GetApiUtils().GetWorld());
		}

		static auto ProcessCharacterImplant(AShooterPlayerController* playerController) -> void
		{
			static UClass* implantClass = UVictoryCore::BPLoadClass("Blueprint'/Game/PrimalEarth/CoreBlueprints/Items/Notes/PrimalItem_StartingNote.PrimalItem_StartingNote'");
			static UProperty* playerAscensionData = PrimalPlayerDataBPBaseCDO->FindProperty(FName("AscensionData", EFindName::FNAME_Find));

			UPrimalPlayerData* playerData;
			AShooterCharacter* playerPawn;
			UPrimalInventoryComponent* playerInventory;
			UPrimalItem* playerImplant;

			if ( /* Guard */
				!implantClass ||
				!playerAscensionData ||
				!(playerData = playerController->GetShooterPlayerState()->MyPlayerDataField()) ||
				!(playerPawn = playerController->LastControlledPlayerCharacterField().Get()) ||
				!(playerInventory = playerPawn->MyInventoryComponentField()) ||
				!(playerImplant = playerInventory->BPGetItemOfTemplate(implantClass, true, false, false, false, false, false, false, false, false, false))
				)
				return;
			/* End Guard */
			FCustomItemData implantData{ .CustomDataName = FName("InitData", EFindName::FNAME_Find) };

			implantData.CustomDataName = FName("InitData", EFindName::FNAME_Find);
			implantData.CustomDataStrings.Add(playerPawn->bIsFemale()() ? "F" : "M");
			implantData.CustomDataStrings.Add(playerController->LinkedPlayerIDString());
			implantData.CustomDataStrings.Add(playerPawn->PlayerNameField());

			implantData.CustomDataFloats = playerAscensionData->Get<TArray<float>>(playerData);

			playerImplant->SetCustomItemData(&implantData);
		}

		static auto onStartNewShooterPlayer(AShooterGameMode* _this, APlayerController* newPlayer, bool forceCreateNewPlayerData, bool isFromLogin, const FPrimalPlayerCharacterConfigStruct& characterConfig, UPrimalPlayerData* playerData)
		{
			AShooterPlayerController* playerController = static_cast<AShooterPlayerController*>(newPlayer);
			StartNewShooterPlayer.original(_this, newPlayer, forceCreateNewPlayerData, isFromLogin, characterConfig, playerData);

			if (!playerController)
				return;

			ProcessUnlocks(playerController);
			ProcessCharacterImplant(playerController);
		}

	public:
		static bool Enable()
		{
			try { config = ReadJSON("ArkApi/Plugins/ArkAutoBosses/config.json").get<Config>();  }
			catch (std::exception& e) { Log::GetLog()->error("Load Error: {}", e.what()); }

			StartNewShooterPlayer.reset(reinterpret_cast<decltype(StartNewShooterPlayer)::element_type*>(GetAddress("AShooterGameMode.StartNewShooterPlayer"))); //Ugly work-around to ensure internal addreses are consistent.
			ArkApi::GetHooks().SetHook("AShooterGameMode.StartNewShooterPlayer", onStartNewShooterPlayer, StartNewShooterPlayer.mut());

			API::Timer::Get().DelayExecute(CacheRuntimeData, 0);
			return true;
		}
		static bool Disable()
		{
			ArkApi::GetHooks().DisableHook("AShooterGameMode.StartNewShooterPlayer", onStartNewShooterPlayer);
			StartNewShooterPlayer.release();
			return true;
		}
	};

}