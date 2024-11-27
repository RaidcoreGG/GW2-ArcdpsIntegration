#include <Windows.h>
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <format>
#include <vector>

#include "nexus/Nexus.h"
#include "arcdps/ArcDPS.h"
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
	ArcDPS::CombatEvent* ev;
	ArcDPS::AgentShort* src;
	ArcDPS::AgentShort* dst;
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

ArcDPS::Exports arc_exports = {};
extern "C" __declspec(dllexport) void* get_init_addr(char* arcversion, void* imguictx, void* id3dptr, HANDLE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion);
extern "C" __declspec(dllexport) void* get_release_addr();
ArcDPS::Exports* ArcdpsInit();
uintptr_t ArcdpsRelease();
void ArcdpsCombatSquad(ArcDPS::CombatEvent* ev, ArcDPS::AgentShort* src, ArcDPS::AgentShort* dst, char* skillname, uint64_t id, uint64_t revision);
void ArcdpsCombatLocal(ArcDPS::CombatEvent* ev, ArcDPS::AgentShort* src, ArcDPS::AgentShort* dst, char* skillname, uint64_t id, uint64_t revision);
void ArcdpsCombat(const char* channel, ArcDPS::CombatEvent* ev, ArcDPS::AgentShort* src, ArcDPS::AgentShort* dst, char* skillname, uint64_t id, uint64_t revision);

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
			APIDefs->Events.Raise("EV_ACCOUNT_NAME", (void*)AccountName.c_str());
		}).detach();
}

void OnSelfRequest(void* eventArgs)
{
	std::scoped_lock lck(Mutex);

	if (Self.id == 0) { return; }

	std::thread([]()
		{
			APIDefs->Events.Raise("EV_ARCDPS_SELF_JOIN", (void*)&Self);
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
				APIDefs->Events.Raise("EV_ARCDPS_SQUAD_JOIN", (void*)&member);
			}
		}).detach();
}

void OnTargetRequest(void* eventArgs)
{
	std::scoped_lock lck(Mutex);
	if (LastTarget.id == NULL) return;
	std::thread([]()
		{
			APIDefs->Events.Raise("EV_ARCDPS_TARGET_CHANGED", (void*)&LastTarget);
		}).detach();
}

void AddonLoad(AddonAPI* aApi)
{
	APIDefs = aApi;
	APIDefs->Events.Subscribe("EV_REQUEST_ACCOUNT_NAME", OnAccountNameRequest);
	APIDefs->Events.Subscribe("EV_REPLAY_ARCDPS_SELF_JOIN", OnSelfRequest);
	APIDefs->Events.Subscribe("EV_REPLAY_ARCDPS_SQUAD_JOIN", OnSquadRequest);
	APIDefs->Events.Subscribe("EV_REPLAY_ARCDPS_TARGET_CHANGED", OnTargetRequest);
}

void AddonUnload()
{
	APIDefs->Events.Unsubscribe("EV_REQUEST_ACCOUNT_NAME", OnAccountNameRequest);
	APIDefs->Events.Unsubscribe("EV_REPLAY_ARCDPS_SELF_JOIN", OnSelfRequest);
	APIDefs->Events.Unsubscribe("EV_REPLAY_ARCDPS_SQUAD_JOIN", OnSquadRequest);
	APIDefs->Events.Unsubscribe("EV_REPLAY_ARCDPS_TARGET_CHANGED", OnTargetRequest);
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
	arc_exports.Signature = -19392669;
	arc_exports.ImGuiVersion = 18000;
	arc_exports.Size = sizeof(ArcDPS::Exports);
	arc_exports.Name= "Nexus ArcDPS Bridge";
	arc_exports.Build = __DATE__ " " __TIME__;
	arc_exports.CombatSquadCallback = ArcdpsCombatSquad;
	arc_exports.CombatLocalCallback = ArcdpsCombatLocal;

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
			evCbtData->src ? evCbtData->src->ID			: 0,
			evCbtData->dst ? evCbtData->dst->ID			: 0,
			evCbtData->src ? evCbtData->src->Profession	: 0,
			evCbtData->src ? evCbtData->src->Specialization	: 0,
			evCbtData->dst ? evCbtData->dst->IsSelf		: 0,
			evCbtData->dst ? evCbtData->dst->Profession	: 0,
			evCbtData->dst ? evCbtData->dst->Specialization	: 0,
			evCbtData->src ? evCbtData->src->Team		: uint16_t(0),
			evCbtData->dst ? evCbtData->dst->Team		: uint16_t(0),
	};
	if (evCbtData->dst && evCbtData->dst->Name && evCbtData->dst->Name[0] != '\0')
	{
		strcpy_s(evAgentUpdate.account, evCbtData->dst->Name);
	}
	if (evCbtData->src && evCbtData->src->Name && evCbtData->src->Name[0] != '\0')
	{
		strcpy_s(evAgentUpdate.character, evCbtData->src->Name);
	}

	if (evAgentUpdate.target)
	{
		std::scoped_lock lck(Mutex);
		LastTarget = evAgentUpdate;
		APIDefs->Events.Raise("EV_ARCDPS_TARGET_CHANGED", &evAgentUpdate);
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
			APIDefs->Events.Raise("EV_ARCDPS_SELF_LEAVE", &evAgentUpdate);
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
						APIDefs->Events.Raise("EV_ARCDPS_SQUAD_LEAVE", &evAgentUpdate);
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
		APIDefs->Events.Raise("EV_ARCDPS_SQUAD_JOIN", &evAgentUpdate);
		return;
	}

	{
		std::scoped_lock lck(Mutex);
		Self = evAgentUpdate;
		APIDefs->Events.Raise("EV_ARCDPS_SELF_JOIN", (void*)&Self);
		if (AccountName.empty()) {
			AccountName = Self.account;
			APIDefs->Events.Raise("EV_ACCOUNT_NAME", (void*)AccountName.c_str());
		}
	}
}

void ArcdpsCombatSquad(ArcDPS::CombatEvent* ev, ArcDPS::AgentShort* src, ArcDPS::AgentShort* dst, char* skillname, uint64_t id, uint64_t revision)
{
	ArcdpsCombat("squad", ev, src, dst, skillname, id, revision);
}

void ArcdpsCombatLocal(ArcDPS::CombatEvent* ev, ArcDPS::AgentShort* src, ArcDPS::AgentShort* dst, char* skillname, uint64_t id, uint64_t revision)
{
	ArcdpsCombat("local", ev, src, dst, skillname, id, revision);
}

void ArcdpsCombat(const char* channel, ArcDPS::CombatEvent* ev, ArcDPS::AgentShort* src, ArcDPS::AgentShort* dst, char* skillname, uint64_t id, uint64_t revision)
{
	if (APIDefs == nullptr) { return; }

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
		APIDefs->Events.Raise("EV_ARCDPS_COMBATEVENT_SQUAD_RAW", &evCbtData);
	}
	else
	{
		APIDefs->Events.Raise("EV_ARCDPS_COMBATEVENT_LOCAL_RAW", &evCbtData);
	}

	AgentUpdate(&evCbtData);
}

void SquadUpdate(const UserInfo* pUpdatedUsers, uint64_t pUpdatedUsersCount)
{
	SquadUpdate_t sqUpdate{
		const_cast<UserInfo*>(pUpdatedUsers),
		pUpdatedUsersCount
	};

	APIDefs->Events.Raise("EV_UNOFFICIAL_EXTRAS_SQUAD_UPDATE", (void*)&sqUpdate);
}

void LanguageChanged(Language pNewLanguage)
{
	APIDefs->Events.Raise("EV_UNOFFICIAL_EXTRAS_LANGUAGE_CHANGED", (void*)&pNewLanguage);
}

void KeyBindChanged(KeyBinds::KeyBindChanged pChangedKeyBind)
{
	APIDefs->Events.Raise("EV_UNOFFICIAL_EXTRAS_KEYBIND_CHANGED", (void*)&pChangedKeyBind);
}

void ChatMessage(const ChatMessageInfo* pChatMessage)
{
	APIDefs->Events.Raise("EV_UNOFFICIAL_EXTRAS_CHAT_MESSAGE", (void*)pChatMessage);
}

extern "C" __declspec(dllexport) void arcdps_unofficial_extras_subscriber_init(const ExtrasAddonInfo* pExtrasInfo, void* pSubscriberInfo) {
	// MaxInfoVersion has to be higher to have enough space to hold this object
	if (pExtrasInfo->ApiVersion == 2 && pExtrasInfo->MaxInfoVersion >= 2)
	{
		if (AccountName.empty()) {
			AccountName = pExtrasInfo->SelfAccountName;
			APIDefs->Events.Raise("EV_ACCOUNT_NAME", (void*)AccountName.c_str());
		}
		ExtrasSubscriberInfoV2* subInfo = (ExtrasSubscriberInfoV2*)(pSubscriberInfo);
		subInfo->InfoVersion = 2;
		subInfo->SubscriberName = "Nexus ArcDPS Bridge";
		subInfo->SquadUpdateCallback = SquadUpdate;
		subInfo->LanguageChangedCallback = LanguageChanged;
		subInfo->KeyBindChangedCallback = KeyBindChanged;
		subInfo->ChatMessageCallback = ChatMessage;
	}
}
