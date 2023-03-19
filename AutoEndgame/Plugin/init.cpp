#pragma once
#include <Logger/Logger.h>
#include "init.h"
#include "plugin.h"

/*
void ReloadConfig(APlayerController* player_controller, FString*, bool)
{
	AShooterPlayerController* shooter_controller = static_cast<AShooterPlayerController*>(player_controller);
	try { ReadConfig(); }
	catch (const std::exception& error)
	{
		ArkApi::GetApiUtils().SendServerMessage(shooter_controller, FColorList::Red, "Failed to reload config");
		Log::GetLog()->error(error.what());
		return;
	}
	ArkApi::GetApiUtils().SendServerMessage(shooter_controller, FColorList::Green, "Reloaded config");
}
*/

BOOL Load()
{
	Log::Get().Init("ArkAutoBosses");
	ArkAutoBosses::Plugin::Enable();
	/*
	ArkApi::GetCommands().AddConsoleCommand("DisallowedNames.Reload", ReloadConfig);
	*/
	return TRUE;
}

BOOL Unload()
{
	ArkAutoBosses::Plugin::Disable();
	/*
	ArkApi::GetCommands().RemoveConsoleCommand("DisallowedNames.Reload");
	*/
	return TRUE;
}
