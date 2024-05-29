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

struct EvAgentUpdateData		// when ev is null
{
	char account[64] = "";		// dst->name	= account name
	char character[64] = "";	// src->name	= character name
	uintptr_t id;				// src->id		= agent id
	uintptr_t instanceId;		// dst->id		= instance id (per map)
	uint32_t added;				// src->prof	= is new agent
	uint32_t target;			// src->elite	= is new targeted agent
	uint32_t self;				// dst->self	= is self
	uint32_t prof;				// dst->prof	= profession / core spec
	uint32_t elite;				// dst->elite	= elite spec
	uint16_t team;				// src->team	= team
	uint16_t subgroup;			// dst->team	= subgroup
};

/* proto / globals */
void AddonLoad(AddonAPI* aApi);
void AddonUnload();

AddonDefinition AddonDef = {};
AddonAPI* APIDefs = nullptr;

std::mutex Mutex;
EvAgentUpdateData evSelfAgentData;

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
	if (evSelfAgentData.id == NULL) return;
	std::thread([&]() {
		APIDefs->RaiseEvent("EV_ARCDPS_SELF_UPDATE", (void*)&evSelfAgentData);
	}).detach();
}

void AddonLoad(AddonAPI* aApi)
{
	APIDefs = aApi;
	APIDefs->SubscribeEvent("EV_ARCDPS_SELF_REQUEST", SelfRequestHandler);
}

void AddonUnload()
{
	APIDefs->UnsubscribeEvent("EV_ARCDPS_SELF_UPDATE", SelfRequestHandler);
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

	if (evCbtData.ev) return 0;
	EvAgentUpdateData evAgentUpdateData {
			"",
			"",
			evCbtData.src->id,    
			evCbtData.dst->id,    
			evCbtData.src->prof,  
			evCbtData.src->elite, 
			evCbtData.dst->self,  
			evCbtData.dst->prof,  
			evCbtData.dst->elite, 
			evCbtData.src->team,  
			evCbtData.dst->team   
	};
	strcpy_s(evAgentUpdateData.account, evCbtData.dst->name);
	strcpy_s(evAgentUpdateData.character, evCbtData.src->name);

	/*APIDefs->Log(ELogLevel_DEBUG, "Nexus Arcdps Bridge", std::format(
		"new agent event via {} channel. id: {}, instanceId: {}, account: {}, added: {}, target: {}, self: {}, prof: {}, elite: {}, team: {}, subgroup: {}",
		channel, evAgentUpdateData.id, evAgentUpdateData.instanceId, evAgentUpdateData.account, evAgentUpdateData.added, evAgentUpdateData.target,
		evAgentUpdateData.self, evAgentUpdateData.prof, evAgentUpdateData.elite, evAgentUpdateData.team, evAgentUpdateData.subgroup
	).c_str());*/
	
	if (!evAgentUpdateData.added)
	{
		APIDefs->RaiseEvent("EV_ARCDPS_SQUAD_UPDATE", &evAgentUpdateData);
		return 0;
	}

	if (evAgentUpdateData.account == nullptr || evAgentUpdateData.account[0] == '\0' ||
		evAgentUpdateData.character == nullptr || evAgentUpdateData.character[0] == '\0') return 0;
	
	if (!evAgentUpdateData.self) {
		APIDefs->RaiseEvent("EV_ARCDPS_SQUAD_UPDATE", &evAgentUpdateData);
		return 0;
	}

	{
		std::scoped_lock lck(Mutex);
		evSelfAgentData = evAgentUpdateData;
		strcpy_s(evSelfAgentData.account, evAgentUpdateData.account);
		strcpy_s(evSelfAgentData.character, evAgentUpdateData.character);
		APIDefs->RaiseEvent("EV_ARCDPS_SELF_UPDATE", (void*)&evSelfAgentData);
	}
	
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
