///----------------------------------------------------------------------------------------------------
/// Copyright (c) Raidcore.GG - Licensed under the MIT License.
///
/// Name         :  Globals.h
/// Description  :  Contains global variables.
///----------------------------------------------------------------------------------------------------

#ifndef GLOBALS_H
#define GLOBALS_H

#include <string>

#include "nexus/Nexus.h"
#include "arcdps/ArcDPS.h"
#include "ArcOptions.hpp"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define ADDON_NAME "Nexus ArcDPS Bridge"

///----------------------------------------------------------------------------------------------------
/// Global Namespace
///----------------------------------------------------------------------------------------------------
namespace G
{
	extern HMODULE LibHandle;
	extern AddonDefinition AddonDef;
	extern AddonAPI*       APIDefs;

	extern ArcDPS::Exports ArcExports;
	extern ArcDPS::Options ArcOptions;

	extern std::string     AccountName;
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

struct EvAgentUpdate       // when ev is null
{
	char account[64];      // dst->name  = account name
	char character[64];    // src->name  = character name
	uintptr_t id;          // src->id    = agent id
	uintptr_t instanceId;  // dst->id    = instance id (per map)
	uint32_t added;        // src->prof  = is new agent
	uint32_t target;       // src->elite = is new targeted agent
	uint32_t Self;         // dst->Self  = is Self
	uint32_t prof;         // dst->prof  = profession / core spec
	uint32_t elite;        // dst->elite = elite spec
	uint16_t team;         // src->team  = team
	uint16_t subgroup;     // dst->team  = subgroup
};


#endif
