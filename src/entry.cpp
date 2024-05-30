#include <Windows.h>
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <format>

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
	char account[64] = "";			// dst->name	= account name
	char character[64] = "";		// src->name	= character name
	uintptr_t id = 0;				// src->id		= agent id
	uintptr_t instanceId = 0;		// dst->id		= instance id (per map)
	uint32_t added = 0;				// src->prof	= is new agent
	uint32_t target = 0;			// src->elite	= is new targeted agent
	uint32_t self = 0;				// dst->self	= is self
	uint32_t prof = 0;				// dst->prof	= profession / core spec
	uint32_t elite = 0;				// dst->elite	= elite spec
	uint16_t team = 0ui16;			// src->team	= team
	uint16_t subgroup = 0ui16;		// dst->team	= subgroup
};

/* proto / globals */
void AddonLoad(AddonAPI* aApi);
void AddonUnload();

AddonDefinition AddonDef = {};
AddonAPI* APIDefs = nullptr;

std::mutex Mutex;
EvAgentUpdate self;

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

void SelfRequestHandler(void* eventArgs) {
	std::scoped_lock lck(Mutex);
	if (self.id == NULL) return;
	std::thread([&]() {
		APIDefs->RaiseEvent("EV_ARCDPS_SELF_DETECT", (void*)&self);
	}).detach();
}

void AddonLoad(AddonAPI* aApi)
{
	APIDefs = aApi;
	APIDefs->SubscribeEvent("EV_ARCDPS_SELF_REQUEST", SelfRequestHandler);
}

void AddonUnload()
{
	APIDefs->UnsubscribeEvent("EV_ARCDPS_SELF_REQUEST", SelfRequestHandler);
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

void mod_agent_update(EvCombatData* evCbtData) {
	if (evCbtData->ev) return;

	EvAgentUpdate evAgentUpdate{
			"",
			"",
			evCbtData->src != nullptr ? evCbtData->src->id : 0,
			evCbtData->dst != nullptr ? evCbtData->dst->id : 0,
			evCbtData->src != nullptr ? evCbtData->src->prof : 0,
			evCbtData->src != nullptr ? evCbtData->src->elite : 0,
			evCbtData->dst != nullptr ? evCbtData->dst->self : 0,
			evCbtData->dst != nullptr ? evCbtData->dst->prof : 0,
			evCbtData->dst != nullptr ? evCbtData->dst->elite : 0,
			evCbtData->src != nullptr ? evCbtData->src->team : 0ui16,
			evCbtData->dst != nullptr ? evCbtData->dst->team : 0ui16,
	};
	if (evCbtData->dst != nullptr && evCbtData->dst->name != nullptr && evCbtData->dst->name[0] != '\0') {
		strcpy_s(evAgentUpdate.account, evCbtData->dst->name);
	}
	if (evCbtData->src != nullptr && evCbtData->src->name != nullptr && evCbtData->src->name[0] != '\0') {
		strcpy_s(evAgentUpdate.character, evCbtData->src->name);
	}

	if (evAgentUpdate.target) {
		APIDefs->RaiseEvent("EV_ARCDPS_TARGET_CHANGED", &evAgentUpdate);
		return;
	}

	/*APIDefs->Log(ELogLevel_DEBUG, "Nexus Arcdps Bridge", std::format(
		"new agent event with id: {}, instanceId: {}, account: {}, added: {}, target: {}, self: {}, prof: {}, elite: {}, team: {}, subgroup: {}",
		evAgentUpdate.id, evAgentUpdate.instanceId, evAgentUpdate.account, evAgentUpdate.added, evAgentUpdate.target,
		evAgentUpdate.self, evAgentUpdate.prof, evAgentUpdate.elite, evAgentUpdate.team, evAgentUpdate.subgroup
	).c_str());*/

	if (!evAgentUpdate.added)
	{
		std::scoped_lock lck(Mutex);
		if (self.id != 0 && self.id == evAgentUpdate.id) {
			// override values to make life easier
			strcpy_s(evAgentUpdate.account, self.account);
			strcpy_s(evAgentUpdate.character, self.character);
			evAgentUpdate.self = 1;
			APIDefs->RaiseEvent("EV_ARCDPS_SELF_LEAVE", &evAgentUpdate);
		}
		else {
			// TODO: cache them all and add names to each leave event
			APIDefs->RaiseEvent("EV_ARCDPS_SQUAD_LEAVE", &evAgentUpdate);
		}
		return;
	}

	if (evAgentUpdate.account == 0 || evAgentUpdate.character == 0) return;

	if (!evAgentUpdate.self) {
		APIDefs->RaiseEvent("EV_ARCDPS_SQUAD_JOIN", &evAgentUpdate);
		return;
	}

	{
		std::scoped_lock lck(Mutex);
		self.id = evAgentUpdate.id;
		self.instanceId = evAgentUpdate.instanceId;
		self.added = evAgentUpdate.added;
		self.target = evAgentUpdate.target;
		self.self = evAgentUpdate.self;
		self.prof = evAgentUpdate.prof;
		self.elite = evAgentUpdate.elite;
		self.team = evAgentUpdate.team;
		self.subgroup = evAgentUpdate.subgroup;
		strcpy_s(self.account, evAgentUpdate.account);
		strcpy_s(self.character, evAgentUpdate.character);
		APIDefs->RaiseEvent("EV_ARCDPS_SELF_DETECT", (void*)&self);
	}
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
	if (APIDefs == nullptr) return 0;
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

	mod_agent_update(&evCbtData);
	
	return 0;
}

struct SquadUpdate_t
{
	UserInfo* UserInfo;
	uint64_t UsersCount;
};

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
		const auto subscriber_info = static_cast<ExtrasSubscriberInfoV2*>(pSubscriberInfo);
		subscriber_info->InfoVersion = 2;
		subscriber_info->SubscriberName = "Nexus ArcDPS Bridge";
		subscriber_info->SquadUpdateCallback = SquadUpdate;
		subscriber_info->LanguageChangedCallback = LanguageChanged;
		subscriber_info->KeyBindChangedCallback = KeyBindChanged;
		subscriber_info->ChatMessageCallback = ChatMessage;
	}
}
