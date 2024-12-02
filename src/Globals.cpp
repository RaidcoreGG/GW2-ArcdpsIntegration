///----------------------------------------------------------------------------------------------------
/// Copyright (c) Raidcore.GG - Licensed under the MIT License.
///
/// Name         :  Globals.cpp
/// Description  :  Contains global variables.
///----------------------------------------------------------------------------------------------------

#include "Globals.h"

namespace G
{
	HMODULE         LibHandle  = nullptr;
	AddonDefinition AddonDef   = {};
	AddonAPI*       APIDefs    = nullptr;

	ArcDPS::Exports ArcExports = {};
	ArcDPS::Options ArcOptions = {};

	std::string     AccountName;
}
