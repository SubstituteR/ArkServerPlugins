#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "init.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        return Load();
    case DLL_PROCESS_DETACH:
        return Unload();
    }
    return FALSE; //Nothing happened!
}

#pragma comment(lib, "ArkApi.lib")