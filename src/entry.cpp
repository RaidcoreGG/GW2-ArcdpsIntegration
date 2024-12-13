///----------------------------------------------------------------------------------------------------
/// Copyright (c) Raidcore.GG - Licensed under the MIT License.
///
/// Name         :  entry.cpp
/// Description  :  The main entry point for the addon.
///----------------------------------------------------------------------------------------------------

#include "entry.h"

#include "Version.h"
#include "resource.h"
#include "Globals.h"
#include "ArcEvents.h"
#include "UeEvents.h"
#include "ExtEvents.h"
#include <filesystem>

constexpr const char* KB_ARCDPS_OPTIONS   = "KB_ARCDPS_OPTIONS";
constexpr const char* ICON_ARCDPS         = "ICON_ARCDPS";
constexpr const char* ICON_ARCDPS_HOVER   = "ICON_ARCDPS_HOVER";
constexpr const char* QA_ARCDPS           = "QA_ARCDPS";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	G::LibHandle = hModule;
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

void LoadArcDPSSettings()
{
	const auto arcIniPath = std::filesystem::path(G::APIDefs->Paths.GetAddonDirectory("arcdps")) / "arcdps.ini";

	G::ArcOptions.global_mod1 = static_cast<unsigned short>(GetPrivateProfileIntA("keys", "global_mod1", VK_SHIFT, arcIniPath.string().c_str()));
	G::ArcOptions.global_mod2 = static_cast<unsigned short>(GetPrivateProfileIntA("keys", "global_mod2", VK_MENU, arcIniPath.string().c_str()));
	G::ArcOptions.global_options = static_cast<unsigned short>(GetPrivateProfileIntA("keys", "global_options", 'T', arcIniPath.string().c_str()));
}

bool IsLegalNexusModifier(unsigned int vk)
{
	return vk == VK_MENU || vk == VK_CONTROL || vk == VK_SHIFT || vk == 0;
}

Keybind GetArcDPSKeybind()
{
	if (G::ArcOptions.global_options && IsLegalNexusModifier(G::ArcOptions.global_options))
	{
		G::APIDefs->Log(ELogLevel_WARNING, ADDON_NAME, "ArcDPS configured to use unsupported toggle options key");
		return {};
	}
	if (!IsLegalNexusModifier(G::ArcOptions.global_mod1))
	{
		G::APIDefs->Log(ELogLevel_WARNING, ADDON_NAME, "ArcDPS configured to use unsupported global_mod1 key");
		return {};
	}
	if (!IsLegalNexusModifier(G::ArcOptions.global_mod2))
	{
		G::APIDefs->Log(ELogLevel_WARNING, ADDON_NAME, "ArcDPS configured to use unsupported global_mod2 key");
		return {};
	}
	return Keybind{
		.Key = static_cast<unsigned short>(MapVirtualKeyA(G::ArcOptions.global_options, MAPVK_VK_TO_VSC)),
		.Alt = G::ArcOptions.global_mod1 == VK_MENU || G::ArcOptions.global_mod2 == VK_MENU,
		.Ctrl = G::ArcOptions.global_mod1 == VK_CONTROL || G::ArcOptions.global_mod2 == VK_CONTROL,
		.Shift = G::ArcOptions.global_mod1 == VK_SHIFT || G::ArcOptions.global_mod2 == VK_SHIFT
	};
}

void ToggleArcDPSOptions(const char*, bool)
{
	G::APIDefs->Log(ELogLevel_DEBUG, ADDON_NAME, "ToggleArcDPSOptions invoked");

	std::vector<INPUT> inputs;
	if (G::ArcOptions.global_mod1)
	{
		inputs.emplace_back(
			INPUT{ .type = INPUT_KEYBOARD, .ki = {.wVk = G::ArcOptions.global_mod1 } }
		);
	}
	if (G::ArcOptions.global_mod2 && G::ArcOptions.global_mod2 != G::ArcOptions.global_mod1)
	{
		inputs.emplace_back(
			INPUT{ .type = INPUT_KEYBOARD, .ki = {.wVk = G::ArcOptions.global_mod2 } }
		);
	}
	if (G::ArcOptions.global_options != G::ArcOptions.global_mod1 && G::ArcOptions.global_options != G::ArcOptions.global_mod2)
	{
		inputs.emplace_back(
			INPUT{ .type = INPUT_KEYBOARD, .ki = {.wVk = G::ArcOptions.global_options } }
		);
	}

	SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));

	for (auto &i : inputs)
	{
		i.ki.dwFlags = KEYEVENTF_KEYUP;
	}

	SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
}

void AddonLoad(AddonAPI* aApi)
{
	G::APIDefs = aApi;
	LoadArcDPSSettings();

	G::APIDefs->Events.Subscribe(EV_REQUEST_ACCOUNT_NAME,         Ext::OnAccountNameRequest);
	G::APIDefs->Events.Subscribe(EV_REPLAY_ARCDPS_SELF_JOIN,      Ext::OnSelfRequest);
	G::APIDefs->Events.Subscribe(EV_REPLAY_ARCDPS_SQUAD_JOIN,     Ext::OnSquadRequest);
	G::APIDefs->Events.Subscribe(EV_REPLAY_ARCDPS_TARGET_CHANGED, Ext::OnTargetRequest);

	G::APIDefs->InputBinds.RegisterWithStruct(KB_ARCDPS_OPTIONS, ToggleArcDPSOptions, GetArcDPSKeybind());
	G::APIDefs->Textures.GetOrCreateFromResource(ICON_ARCDPS, RES_ICON_ARCDPS, G::LibHandle);
	G::APIDefs->Textures.GetOrCreateFromResource(ICON_ARCDPS_HOVER, RES_ICON_ARCDPS_HOVER, G::LibHandle);
	G::APIDefs->QuickAccess.Add(QA_ARCDPS, ICON_ARCDPS, ICON_ARCDPS_HOVER, KB_ARCDPS_OPTIONS, KB_ARCDPS_OPTIONS);
	G::APIDefs->Localization.Set(KB_ARCDPS_OPTIONS, "en", "Toggle ArcDPS Options");
	G::APIDefs->Localization.Set(KB_ARCDPS_OPTIONS, "de", "ArcDPS Optionen umschalten");
	//G::APIDefs->Localization.Set(KB_ARCDPS_OPTIONS, "es", "Toggle ArcDPS Options");
	//G::APIDefs->Localization.Set(KB_ARCDPS_OPTIONS, "fr", "Toggle ArcDPS Options");
}

void AddonUnload()
{
	G::APIDefs->Events.Unsubscribe(EV_REQUEST_ACCOUNT_NAME,         Ext::OnAccountNameRequest);
	G::APIDefs->Events.Unsubscribe(EV_REPLAY_ARCDPS_SELF_JOIN,      Ext::OnSelfRequest);
	G::APIDefs->Events.Unsubscribe(EV_REPLAY_ARCDPS_SQUAD_JOIN,     Ext::OnSquadRequest);
	G::APIDefs->Events.Unsubscribe(EV_REPLAY_ARCDPS_TARGET_CHANGED, Ext::OnTargetRequest);
	G::APIDefs->InputBinds.Deregister(KB_ARCDPS_OPTIONS);
	G::APIDefs->QuickAccess.Remove(QA_ARCDPS);
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
