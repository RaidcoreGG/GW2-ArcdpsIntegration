#include <Windows.h>
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <format>
#include <vector>

#include "nexus/Nexus.h"
#include "ArcDPS.h"
#include "unofficial_extras/Definitions.h"

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

struct EvAgentUpdate				// when ev is null
{
	char account[64];		// dst->name	= account name
	char character[64];		// src->name	= character name
	uintptr_t id;			// src->id		= agent id
	uintptr_t instanceId;	// dst->id		= instance id (per map)
	uint32_t added;			// src->prof	= is new agent
	uint32_t target;		// src->elite	= is new targeted agent
	uint32_t Self;			// dst->Self	= is Self
	uint32_t prof;			// dst->prof	= profession / core spec
	uint32_t elite;			// dst->elite	= elite spec
	uint16_t team;			// src->team	= team
	uint16_t subgroup;		// dst->team	= subgroup
};

struct SquadUpdate_t
{
	UserInfo* UserInfo;
	uint64_t UsersCount;
};

/* proto / globals */
void AddonLoad(AddonAPI* aApi);
void AddonUnload();

AddonDefinition AddonDef = {};
AddonAPI* APIDefs = nullptr;

std::mutex Mutex;
std::string AccountName;
EvAgentUpdate Self;
EvAgentUpdate LastTarget;
std::vector<EvAgentUpdate> Squad;

arcdps_exports arc_exports = {};
extern "C" __declspec(dllexport) void* get_init_addr(char* arcversion, void* imguictx, void* id3dptr, HANDLE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion);
extern "C" __declspec(dllexport) void* get_release_addr();
arcdps_exports* ArcdpsInit();
uintptr_t ArcdpsRelease();
uintptr_t ArcdpsCombatSquad(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision);
uintptr_t ArcdpsCombatLocal(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision);
uintptr_t ArcdpsCombat(const char* channel, cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision);

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

void OnAccountNameRequest(void* eventArgs)
{
	std::scoped_lock lck(Mutex);

	if (AccountName.empty()) { return; }

	std::thread([]()
		{
			APIDefs->RaiseEvent("EV_ACCOUNT_NAME", (void*)AccountName.c_str());
		}).detach();
}

void OnSelfRequest(void* eventArgs)
{
	std::scoped_lock lck(Mutex);

	if (Self.id == 0) { return; }

	std::thread([]()
		{
			APIDefs->RaiseEvent("EV_ARCDPS_SELF_JOIN", (void*)&Self);
		}).detach();
}

void OnSquadRequest(void* eventArgs)
{
	std::scoped_lock lck(Mutex);

	if (Squad.size() == 0) { return; }

	std::thread([]()
		{
			for (EvAgentUpdate& member : Squad)
			{
				APIDefs->RaiseEvent("EV_ARCDPS_SQUAD_JOIN", (void*)&member);
			}
		}).detach();
}

void OnTargetRequest(void* eventArgs)
{
	std::scoped_lock lck(Mutex);
	if (LastTarget.id == NULL) return;
	std::thread([]()
		{
			APIDefs->RaiseEvent("EV_ARCDPS_TARGET_CHANGED", (void*)&LastTarget);
		}).detach();
}

void AddonLoad(AddonAPI* aApi)
{
	APIDefs = aApi;
	APIDefs->SubscribeEvent("EV_REQUEST_ACCOUNT_NAME", OnAccountNameRequest);
	APIDefs->SubscribeEvent("EV_REPLAY_ARCDPS_SELF_JOIN", OnSelfRequest);
	APIDefs->SubscribeEvent("EV_REPLAY_ARCDPS_SQUAD_JOIN", OnSquadRequest);
	APIDefs->SubscribeEvent("EV_REPLAY_ARCDPS_TARGET_CHANGED", OnTargetRequest);
}

void AddonUnload()
{
	APIDefs->UnsubscribeEvent("EV_REQUEST_ACCOUNT_NAME", OnAccountNameRequest);
	APIDefs->UnsubscribeEvent("EV_REPLAY_ARCDPS_SELF_JOIN", OnSelfRequest);
	APIDefs->UnsubscribeEvent("EV_REPLAY_ARCDPS_SQUAD_JOIN", OnSquadRequest);
	APIDefs->UnsubscribeEvent("EV_REPLAY_ARCDPS_TARGET_CHANGED", OnTargetRequest);
	return;
}

extern "C" __declspec(dllexport) void* get_init_addr(char* arcversion, void* imguictx, void* id3dptr, HANDLE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion)
{
	return ArcdpsInit;
}

extern "C" __declspec(dllexport) void* get_release_addr()
{
	return ArcdpsRelease;
}

arcdps_exports* ArcdpsInit()
{
	arc_exports.sig = -19392669;
	arc_exports.imguivers = 18000;
	arc_exports.size = sizeof(arcdps_exports);
	arc_exports.out_name = "Nexus ArcDPS Bridge";
	arc_exports.out_build = __DATE__ " " __TIME__;
	arc_exports.combat = ArcdpsCombatSquad;
	arc_exports.combat_local = ArcdpsCombatLocal;

	return &arc_exports;
}

uintptr_t ArcdpsRelease()
{
	return 0;
}

void AgentUpdate(EvCombatData* evCbtData)
{
	if (evCbtData->ev) { return; }

	EvAgentUpdate evAgentUpdate
	{
			"",
			"",
			evCbtData->src ? evCbtData->src->id		: 0,
			evCbtData->dst ? evCbtData->dst->id		: 0,
			evCbtData->src ? evCbtData->src->prof	: 0,
			evCbtData->src ? evCbtData->src->elite	: 0,
			evCbtData->dst ? evCbtData->dst->self	: 0,
			evCbtData->dst ? evCbtData->dst->prof	: 0,
			evCbtData->dst ? evCbtData->dst->elite	: 0,
			evCbtData->src ? evCbtData->src->team	: 0,
			evCbtData->dst ? evCbtData->dst->team	: 0,
	};
	if (evCbtData->dst && evCbtData->dst->name && evCbtData->dst->name[0] != '\0')
	{
		strcpy_s(evAgentUpdate.account, evCbtData->dst->name);
	}
	if (evCbtData->src && evCbtData->src->name && evCbtData->src->name[0] != '\0')
	{
		strcpy_s(evAgentUpdate.character, evCbtData->src->name);
	}

	if (evAgentUpdate.target)
	{
		std::scoped_lock lck(Mutex);
		LastTarget = evAgentUpdate;
		APIDefs->RaiseEvent("EV_ARCDPS_TARGET_CHANGED", &evAgentUpdate);
		return;
	}

	/*APIDefs->Log(ELogLevel_DEBUG, "Nexus Arcdps Bridge", std::format(
		"new agent event with id: {}, instanceId: {}, account: {}, added: {}, target: {}, Self: {}, prof: {}, elite: {}, team: {}, subgroup: {}",
		evAgentUpdate.id, evAgentUpdate.instanceId, evAgentUpdate.account, evAgentUpdate.added, evAgentUpdate.target,
		evAgentUpdate.Self, evAgentUpdate.prof, evAgentUpdate.elite, evAgentUpdate.team, evAgentUpdate.subgroup
	).c_str());*/

	if (!evAgentUpdate.added)
	{
		std::scoped_lock lck(Mutex);
		if (Self.id != 0 && Self.id == evAgentUpdate.id)
		{
			strcpy_s(evAgentUpdate.account, Self.account);
			strcpy_s(evAgentUpdate.character, Self.character);
			evAgentUpdate.Self = 1;
			Squad.clear();
			APIDefs->RaiseEvent("EV_ARCDPS_SELF_LEAVE", &evAgentUpdate);
		}
		else
		{
			Squad.erase(std::remove_if(
				Squad.begin(), Squad.end(),
				[&](EvAgentUpdate const &member)
				{
					if (member.id == evAgentUpdate.id)
					{
						strcpy_s(evAgentUpdate.account, member.account);
						strcpy_s(evAgentUpdate.character, member.character);
						APIDefs->RaiseEvent("EV_ARCDPS_SQUAD_LEAVE", &evAgentUpdate);
						return true;
					}
					return false;
				}),
				Squad.end()
			);
		}
		return;
	}

	if (evAgentUpdate.account == 0 || evAgentUpdate.character == 0) { return; }

	if (!evAgentUpdate.Self)
	{
		std::scoped_lock lck(Mutex);
		Squad.push_back(evAgentUpdate);
		APIDefs->RaiseEvent("EV_ARCDPS_SQUAD_JOIN", &evAgentUpdate);
		return;
	}

	{
		std::scoped_lock lck(Mutex);
		Self = evAgentUpdate;
		APIDefs->RaiseEvent("EV_ARCDPS_SELF_JOIN", (void*)&Self);
		if (AccountName.empty()) {
			AccountName = Self.account;
			APIDefs->RaiseEvent("EV_ACCOUNT_NAME", (void*)AccountName.c_str());
		}
	}
}

uintptr_t ArcdpsCombatSquad(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision)
{
	return ArcdpsCombat("squad", ev, src, dst, skillname, id, revision);
}

uintptr_t ArcdpsCombatLocal(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision)
{
	return ArcdpsCombat("local", ev, src, dst, skillname, id, revision);
}

uintptr_t ArcdpsCombat(const char* channel, cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision)
{
	if (APIDefs == nullptr) { return 0; }

	EvCombatData evCbtData
	{
		ev,
		src,
		dst,
		skillname,
		id,
		revision
	};

	if (strcmp(channel, "squad") == 0)
	{
		APIDefs->RaiseEvent("EV_ARCDPS_COMBATEVENT_SQUAD_RAW", &evCbtData);
	}
	else
	{
		APIDefs->RaiseEvent("EV_ARCDPS_COMBATEVENT_LOCAL_RAW", &evCbtData);
	}

	AgentUpdate(&evCbtData);
	
	return 0;
}

void SquadUpdate(const UserInfo* pUpdatedUsers, uint64_t pUpdatedUsersCount)
{
	SquadUpdate_t sqUpdate{
		const_cast<UserInfo*>(pUpdatedUsers),
		pUpdatedUsersCount
	};

	APIDefs->RaiseEvent("EV_UNOFFICIAL_EXTRAS_SQUAD_UPDATE", (void*)&sqUpdate);
}

void LanguageChanged(Language pNewLanguage)
{
	APIDefs->RaiseEvent("EV_UNOFFICIAL_EXTRAS_LANGUAGE_CHANGED", (void*)&pNewLanguage);
}

void KeyBindChanged(KeyBinds::KeyBindChanged pChangedKeyBind)
{
	APIDefs->RaiseEvent("EV_UNOFFICIAL_EXTRAS_KEYBIND_CHANGED", (void*)&pChangedKeyBind);
}

void ChatMessage(const ChatMessageInfo* pChatMessage)
{
	APIDefs->RaiseEvent("EV_UNOFFICIAL_EXTRAS_CHAT_MESSAGE", (void*)pChatMessage);
}

extern "C" __declspec(dllexport) void arcdps_unofficial_extras_subscriber_init(const ExtrasAddonInfo* pExtrasInfo, void* pSubscriberInfo) {
	// MaxInfoVersion has to be higher to have enough space to hold this object
	if (pExtrasInfo->ApiVersion == 2 && pExtrasInfo->MaxInfoVersion >= 2)
	{
		AccountName = pExtrasInfo->SelfAccountName;
		ExtrasSubscriberInfoV2* subInfo = (ExtrasSubscriberInfoV2*)(pSubscriberInfo);
		subInfo->InfoVersion = 2;
		subInfo->SubscriberName = "Nexus ArcDPS Bridge";
		subInfo->SquadUpdateCallback = SquadUpdate;
		subInfo->LanguageChangedCallback = LanguageChanged;
		subInfo->KeyBindChangedCallback = KeyBindChanged;
		subInfo->ChatMessageCallback = ChatMessage;
	}
}
