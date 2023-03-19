#pragma once
#include <fstream>
#include <iostream>
#include <filesystem>
#include <unordered_set>
#include <moar_ptr.h>
#include <API/ARK/Ark.h>
#include <Timer.h>
#include "json.hpp"

namespace AutoEndgame
{
	using json = nlohmann::json;

	class Plugin
	{
		static inline moar::function_ptr<void(AShooterGameMode*, APlayerController*, bool, bool, const FPrimalPlayerCharacterConfigStruct&, UPrimalPlayerData*)> StartNewShooterPlayer;
		static inline moar::function_ptr<void(UPrimalInventoryComponent*, UPrimalItem*, bool)> NotifyItemAdded;
		static inline moar::function_ptr<void(UPrimalInventoryComponent*, UPrimalItem*)> NotifyItemRemoved;

		static inline UClass* AwesomeSpyGlassBuffClass;
		static inline UClass* AwesomeSpyGlassClass;

		static auto CacheRuntimeData() -> void
		{
			AwesomeSpyGlassBuffClass = UVictoryCore::BPLoadClass("Blueprint'/Game/Mods/AwesomeSpyglass/AwesomeSpyGlass_Buff.AwesomeSpyGlass_Buff'");
			AwesomeSpyGlassClass = UVictoryCore::BPLoadClass("Blueprint'/Game/Mods/AwesomeSpyglass/PrimalItem_AwesomeSpyGlass.PrimalItem_AwesomeSpyGlass'");
			Log::GetLog()->info("Cached AwesomeSpyGlassBuffClass. ({})", (void*)AwesomeSpyGlassBuffClass);
			Log::GetLog()->info("Cached AwesomeSpyGlassClass. ({})", (void*)AwesomeSpyGlassClass);
		}


		static auto EnableSpyGlass(UPrimalInventoryComponent* playerInventory, UPrimalItem* spyglass, bool SkipIfEnabled)
		{
			if(!playerInventory->GetOwner() || !playerInventory->GetOwner()->IsA(AShooterCharacter::StaticClass()))
				return;

			AShooterCharacter* playerCharacter = static_cast<AShooterCharacter*>(playerInventory->GetOwner());

			if (playerCharacter->HasBuff(AwesomeSpyGlassBuffClass, true) && SkipIfEnabled)
				return;

			playerCharacter->DeactivateBuffs(AwesomeSpyGlassBuffClass, spyglass, true);
			spyglass->Use(true);

		}

		static auto FindAndEnableSpyGlass(UPrimalInventoryComponent* playerInventory, bool SkipIfEnabled) -> void
		{
			for (auto&& item : playerInventory->InventoryItemsField())
			{
				if (item->bIsEngram()() || item->bIsBlueprint()() || !item->IsA(AwesomeSpyGlassClass))
					continue;
				
				EnableSpyGlass(playerInventory, item, SkipIfEnabled);

				return;
			}
		}

		static auto onNotifyItemAdded(UPrimalInventoryComponent* _this, UPrimalItem* anItem, bool bEquipItem) -> void
		{
			NotifyItemAdded.original(_this, anItem, bEquipItem);
			if (!_this ||!anItem || !anItem->IsA(AwesomeSpyGlassClass))
				return;
			EnableSpyGlass(_this, anItem, true);
		}

		static auto onNotifyItemRemoved(UPrimalInventoryComponent* _this, UPrimalItem* anItem) -> void
		{
			NotifyItemRemoved.original(_this, anItem);
			if (!_this || !anItem || !anItem->IsA(AwesomeSpyGlassClass))
				return;
			API::Timer::Get().DelayExecute(std::bind(FindAndEnableSpyGlass, _this, false), 0);
		}

		static auto onStartNewShooterPlayer(AShooterGameMode* _this, APlayerController* newPlayer, bool forceCreateNewPlayerData, bool isFromLogin, const FPrimalPlayerCharacterConfigStruct& characterConfig, UPrimalPlayerData* playerData)
		{
			StartNewShooterPlayer.original(_this, newPlayer, forceCreateNewPlayerData, isFromLogin, characterConfig, playerData);

			AShooterPlayerController* playerController;
			UPrimalInventoryComponent* playerInventory;

			if ( /* Guard */
				!(playerController = static_cast<AShooterPlayerController*>(newPlayer)) || 
				!(playerInventory = playerController->GetPlayerInventory())
				)
				return;
			/* End Guard */

			API::Timer::Get().DelayExecute(std::bind(FindAndEnableSpyGlass, playerInventory, false), 0);
		}

	public:
		static auto Enable() -> bool
		{
			StartNewShooterPlayer.reset(reinterpret_cast<decltype(StartNewShooterPlayer)::element_type*>(GetAddress("AShooterGameMode.StartNewShooterPlayer"))); //Ugly work-around to ensure internal addreses are consistent.
			NotifyItemAdded.reset(reinterpret_cast<decltype(NotifyItemAdded)::element_type*>(GetAddress("UPrimalInventoryComponent.NotifyItemAdded")));
			NotifyItemRemoved.reset(reinterpret_cast<decltype(NotifyItemRemoved)::element_type*>(GetAddress("UPrimalInventoryComponent.NotifyItemAdded")));

			ArkApi::GetHooks().SetHook("AShooterGameMode.StartNewShooterPlayer", onStartNewShooterPlayer, StartNewShooterPlayer.mut());
			ArkApi::GetHooks().SetHook("UPrimalInventoryComponent.NotifyItemAdded", onNotifyItemAdded, NotifyItemAdded.mut());
			ArkApi::GetHooks().SetHook("UPrimalInventoryComponent.NotifyItemRemoved", onNotifyItemRemoved, NotifyItemRemoved.mut());

			API::Timer::Get().DelayExecute(CacheRuntimeData, 0);
			return true;
		}

		static auto Disable() -> bool
		{
			ArkApi::GetHooks().DisableHook("UPrimalInventoryComponent.NotifyItemRemoved", onNotifyItemRemoved);
			ArkApi::GetHooks().DisableHook("UPrimalInventoryComponent.NotifyItemAdded", onNotifyItemAdded);
			ArkApi::GetHooks().DisableHook("AShooterGameMode.StartNewShooterPlayer", onStartNewShooterPlayer);
			NotifyItemRemoved.release();
			NotifyItemAdded.release();
			StartNewShooterPlayer.release();

			return true;
		}
	};

}