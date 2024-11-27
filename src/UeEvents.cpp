///----------------------------------------------------------------------------------------------------
/// Copyright (c) Raidcore.GG - Licensed under the MIT License.
///
/// Name         :  UeEvents.cpp
/// Description  :  Contains the callbacks for unofficial extras.
///----------------------------------------------------------------------------------------------------

#include "UeEvents.h"

#include "Globals.h"

namespace UE
{
	void OnSquadUpdate(const UserInfo* aUpdatedUsers, uint64_t aAmtUpdatedUsers)
	{
		if (!G::APIDefs) { return; }

		struct SquadUpdate_t
		{
			UserInfo* UserInfo;
			uint64_t  UsersCount;
		};

		SquadUpdate_t sqUpdate
		{
			const_cast<UserInfo*>(aUpdatedUsers),
			aAmtUpdatedUsers
		};

		G::APIDefs->Events.Raise("EV_UNOFFICIAL_EXTRAS_SQUAD_UPDATE", &sqUpdate);
	}

	void OnLanguageChanged(Language aLanguage)
	{
		if (!G::APIDefs) { return; }

		G::APIDefs->Events.Raise("EV_UNOFFICIAL_EXTRAS_LANGUAGE_CHANGED", &aLanguage);
	}

	void OnKeyBindChanged(KeyBinds::KeyBindChanged aKeyBindChange)
	{
		if (!G::APIDefs) { return; }

		G::APIDefs->Events.Raise("EV_UNOFFICIAL_EXTRAS_KEYBIND_CHANGED", &aKeyBindChange);
	}

	void OnChatMessage(const ChatMessageInfo* aChatMessage)
	{
		if (!G::APIDefs) { return; }

		G::APIDefs->Events.Raise("EV_UNOFFICIAL_EXTRAS_CHAT_MESSAGE", (void*)aChatMessage);
	}
}
