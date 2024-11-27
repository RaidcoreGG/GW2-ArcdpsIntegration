///----------------------------------------------------------------------------------------------------
/// Copyright (c) Raidcore.GG - Licensed under the MIT License.
///
/// Name         :  entry.cpp
/// Description  :  The main entry point for the addon.
///----------------------------------------------------------------------------------------------------

#include "entry.h"

#include "Version.h"
#include "Globals.h"
#include "ArcEvents.h"
#include "UeEvents.h"
#include "ExtEvents.h"

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

extern "C" __declspec(dllexport) AddonDefinition* GetAddonDef()
{
	G::AddonDef.Signature        = -19392669;
	G::AddonDef.APIVersion       = NEXUS_API_VERSION;
	G::AddonDef.Name             = ADDON_NAME;
	G::AddonDef.Version.Major    = V_MAJOR;
	G::AddonDef.Version.Minor    = V_MINOR;
	G::AddonDef.Version.Build    = V_BUILD;
	G::AddonDef.Version.Revision = V_REVISION;
	G::AddonDef.Author           = "Raidcore";
	G::AddonDef.Description      = "Raises Nexus Events from ArcDPS combat API callbacks.";
	G::AddonDef.Load             = AddonLoad;
	G::AddonDef.Unload           = AddonUnload;
	G::AddonDef.Flags            = EAddonFlags_DisableHotloading;

	return &G::AddonDef;
}

void AddonLoad(AddonAPI* aApi)
{
	G::APIDefs = aApi;

	G::APIDefs->Events.Subscribe(EV_REQUEST_ACCOUNT_NAME,         Ext::OnAccountNameRequest);
	G::APIDefs->Events.Subscribe(EV_REPLAY_ARCDPS_SELF_JOIN,      Ext::OnSelfRequest);
	G::APIDefs->Events.Subscribe(EV_REPLAY_ARCDPS_SQUAD_JOIN,     Ext::OnSquadRequest);
	G::APIDefs->Events.Subscribe(EV_REPLAY_ARCDPS_TARGET_CHANGED, Ext::OnTargetRequest);
}

void AddonUnload()
{
	G::APIDefs->Events.Unsubscribe(EV_REQUEST_ACCOUNT_NAME,         Ext::OnAccountNameRequest);
	G::APIDefs->Events.Unsubscribe(EV_REPLAY_ARCDPS_SELF_JOIN,      Ext::OnSelfRequest);
	G::APIDefs->Events.Unsubscribe(EV_REPLAY_ARCDPS_SQUAD_JOIN,     Ext::OnSquadRequest);
	G::APIDefs->Events.Unsubscribe(EV_REPLAY_ARCDPS_TARGET_CHANGED, Ext::OnTargetRequest);
}

extern "C" __declspec(dllexport) void* get_init_addr(char* arcversion, void* imguictx, void* id3dptr, HANDLE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion)
{
	return ArcdpsInit;
}

extern "C" __declspec(dllexport) void* get_release_addr()
{
	return ArcdpsRelease;
}

ArcDPS::Exports* ArcdpsInit()
{
	G::ArcExports.Signature           = -19392669;
	G::ArcExports.ImGuiVersion        = 18000;
	G::ArcExports.Size                = sizeof(ArcDPS::Exports);
	G::ArcExports.Name                = ADDON_NAME;
	G::ArcExports.Build               = TOSTRING(V_MAJOR) "." TOSTRING(V_MINOR) "." TOSTRING(V_BUILD) "." TOSTRING(V_REVISION);
	G::ArcExports.CombatSquadCallback = ArcEv::OnCombatSquad;
	G::ArcExports.CombatLocalCallback = ArcEv::OnCombatLocal;

	return &G::ArcExports;
}

uintptr_t ArcdpsRelease()
{
	return 0;
}

extern "C" __declspec(dllexport) void arcdps_unofficial_extras_subscriber_init(const ExtrasAddonInfo* aApi, ExtrasSubscriberInfoV2* aUEInfoPtr)
{
	// MaxInfoVersion has to be higher to have enough space to hold this object
	if (aApi->ApiVersion == 2 && aApi->MaxInfoVersion >= 2)
	{
		if (G::AccountName.empty())
		{
			G::AccountName = aApi->SelfAccountName;
			G::APIDefs->Events.Raise(EV_ACCOUNT_NAME, (void*)G::AccountName.c_str());
		}

		aUEInfoPtr->InfoVersion             = 2;
		aUEInfoPtr->SubscriberName          = ADDON_NAME;
		aUEInfoPtr->SquadUpdateCallback     = UE::OnSquadUpdate;
		aUEInfoPtr->LanguageChangedCallback = UE::OnLanguageChanged;
		aUEInfoPtr->KeyBindChangedCallback  = UE::OnKeyBindChanged;
		aUEInfoPtr->ChatMessageCallback     = UE::OnChatMessage;

		return;
	}

	if (G::APIDefs)
	{
		G::APIDefs->Log(ELogLevel_WARNING, ADDON_NAME, "Unofficial Extras API Version mismatch.");
	}
	else
	{
		ArcDPS::Log((char*)"Unofficial Extras API Version mismatch.\nNexus logging unavailable.");
	}
}
