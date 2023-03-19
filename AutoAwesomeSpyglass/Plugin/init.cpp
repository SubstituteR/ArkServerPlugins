#pragma once
#include <Logger/Logger.h>
#include "init.h"
#include "plugin.h"

BOOL Load()
{
	Log::Get().Init("AutoAwesomeSpyglass");
	AutoEndgame::Plugin::Enable();
	return TRUE;
}

BOOL Unload()
{
	AutoEndgame::Plugin::Disable();
	return TRUE;
}
