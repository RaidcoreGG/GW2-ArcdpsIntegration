#include <Windows.h>
#include <vector>
#include <string>
#include <mutex>

#include "nexus/Nexus.h"
#include "ArcDPS.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

struct EvCombatData
{
	cbtevent* ev;
	ag* src;
	ag* dst;
	char* skillname;
	uint64_t id;
	uint64_t revision;
};

/* proto / globals */
void AddonLoad(AddonAPI* aApi);
void AddonUnload();

AddonDefinition AddonDef = {};
AddonAPI* APIDefs = nullptr;

arcdps_exports arc_exports = {};
uint32_t cbtcount = 0;
extern "C" __declspec(dllexport) void* get_init_addr(char* arcversion, void* imguictx, void* id3dptr, HANDLE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion);
extern "C" __declspec(dllexport) void* get_release_addr();
arcdps_exports* mod_init();
uintptr_t mod_release();
uintptr_t mod_combat_squad(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision);
uintptr_t mod_combat_local(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision);
uintptr_t mod_combat(const char* channel, cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision);

extern "C" __declspec(dllexport) AddonDefinition* GetAddonDef()
{
	AddonDef.Signature = -19392669;
	AddonDef.APIVersion = NEXUS_API_VERSION;
	AddonDef.Name = "Nexus ArcDPS Bridge";
	AddonDef.Version.Major = 1;
	AddonDef.Version.Minor = 0;
	AddonDef.Version.Build = 0;
	AddonDef.Version.Revision = 1;
	AddonDef.Author = "Raidcore";
	AddonDef.Description = "Raises Nexus Events from ArcDPS combat API callbacks.";
	AddonDef.Load = AddonLoad;
	AddonDef.Unload = AddonUnload;
	AddonDef.Flags = EAddonFlags_DisableHotloading;

	return &AddonDef;
}

void AddonLoad(AddonAPI* aApi)
{
	APIDefs = aApi;
}

void AddonUnload()
{
	return;
}

extern "C" __declspec(dllexport) void* get_init_addr(char* arcversion, void* imguictx, void* id3dptr, HANDLE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion)
{
	return mod_init;
}

extern "C" __declspec(dllexport) void* get_release_addr()
{
	return mod_release;
}

arcdps_exports* mod_init()
{
	arc_exports.sig = -19392669;
	arc_exports.imguivers = 18000;
	arc_exports.size = sizeof(arcdps_exports);
	arc_exports.out_name = "Nexus ArcDPS Bridge";
	arc_exports.out_build = __DATE__ " " __TIME__;
	arc_exports.combat = mod_combat_squad;
	arc_exports.combat_local = mod_combat_local;

	return &arc_exports;
}

uintptr_t mod_release()
{
	return 0;
}

uintptr_t mod_combat_squad(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision)
{
	return mod_combat("squad", ev, src, dst, skillname, id, revision);
}

uintptr_t mod_combat_local(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision)
{
	return mod_combat("local", ev, src, dst, skillname, id, revision);
}

uintptr_t mod_combat(const char* channel, cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision)
{
	EvCombatData evCbtData
	{
		ev,
		src,
		dst,
		skillname,
		id,
		revision
	};

	if (APIDefs != nullptr)
	{
		if (strcmp(channel, "local") == 0)
		{
			APIDefs->RaiseEvent("EV_ARCDPS_COMBATEVENT_LOCAL_RAW", &evCbtData);
		}
		else
		{
			APIDefs->RaiseEvent("EV_ARCDPS_COMBATEVENT_SQUAD_RAW", &evCbtData);
		}
	}

	return 0;
}