///----------------------------------------------------------------------------------------------------
/// Copyright (c) Raidcore.GG - Licensed under the MIT License.
///
/// Name         :  UeEvents.h
/// Description  :  Contains the callbacks for unofficial extras.
///----------------------------------------------------------------------------------------------------

#ifndef UEEVENTS_H
#define UEEVENTS_H

#include "unofficial_extras/Definitions.h"

///----------------------------------------------------------------------------------------------------
/// UE Namespace
///----------------------------------------------------------------------------------------------------
namespace UE
{
	///----------------------------------------------------------------------------------------------------
	/// OnSquadUpdate:
	/// 	Receives information about changes in the squad/party.
	///----------------------------------------------------------------------------------------------------
	void OnSquadUpdate(const UserInfo* aUpdatedUsers, uint64_t aAmtUpdatedUsers);

	///----------------------------------------------------------------------------------------------------
	/// OnLanguageChanged:
	/// 	Receives the language the game was set to.
	///----------------------------------------------------------------------------------------------------
	void OnLanguageChanged(Language aLanguage);

	///----------------------------------------------------------------------------------------------------
	/// OnKeyBindChanged:
	/// 	Receives keybind changes.
	///----------------------------------------------------------------------------------------------------
	void OnKeyBindChanged(KeyBinds::KeyBindChanged aKeyBindChange);

	///----------------------------------------------------------------------------------------------------
	/// OnChatMessage:
	/// 	Receives squad chat messages.
	///----------------------------------------------------------------------------------------------------
	void OnChatMessage(const ChatMessageInfo* aChatMessage);
}

#endif
