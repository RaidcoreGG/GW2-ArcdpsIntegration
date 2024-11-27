///----------------------------------------------------------------------------------------------------
/// Copyright (c) Raidcore.GG - Licensed under the MIT License.
///
/// Name         :  entry.h
/// Description  :  The main entry point for the addon.
///----------------------------------------------------------------------------------------------------

#ifndef ENTRY_H
#define ENTRY_H

#include <Windows.h>

#include "nexus/Nexus.h"
#include "arcdps/ArcDPS.h"
#include "unofficial_extras/Definitions.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved);

extern "C" __declspec(dllexport) AddonDefinition* GetAddonDef();

void AddonLoad(AddonAPI* aApi);

void AddonUnload();

extern "C" __declspec(dllexport) void* get_init_addr(
	char* arcversion,
	void* imguictx,
	void* id3dptr,
	HANDLE arcdll,
	void* mallocfn,
	void* freefn,
	uint32_t d3dversion
);

extern "C" __declspec(dllexport) void* get_release_addr();

ArcDPS::Exports* ArcdpsInit();

uintptr_t ArcdpsRelease();

extern "C" __declspec(dllexport) void arcdps_unofficial_extras_subscriber_init(const ExtrasAddonInfo* aApi, ExtrasSubscriberInfoV2* aUEInfoPtr);

#endif
