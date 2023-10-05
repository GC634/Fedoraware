#include "AutoQueue.h"

void CAutoQueue::Run()
{
	// Check if the player is in game.
	const bool bInGame = (!I::EngineVGui->IsGameUIVisible() || I::EngineClient->IsInGame());

	// Auto queue.
	if (Vars::Misc::AutoCasualQueue.Value == 1)
	{
		// If the player is in game, return.
		if (bInGame)
		{
			return;
		}

		// Check if the player is in standby queue, has a live match, or has any match invites.
		const bool bInStandbyQueue = I::TFPartyClient->BInStandbyQueue();
		const bool bHaveLiveMatch = I::TFGCClientSystem->BHaveLiveMatch();
		const int nNumMatchInvites = I::TFGCClientSystem->GetNumMatchInvites();

		// If the player is not in standby queue, does not have a live match, and does not have any match invites, queue for a casual match.
		if (!bInStandbyQueue &&
			!bHaveLiveMatch &&
			!nNumMatchInvites)
		{
			I::TFPartyClient->LoadSavedCasualCriteria();
			I::TFPartyClient->RequestQueueForMatch(k_eTFMatchGroup_Casual_Default);
		}
	}
	else if (Vars::Misc::AutoCasualQueue.Value == 2)
	{
		// Check if the player is in queue for a casual match.
		const bool bInQueueForMatchGroup = I::TFPartyClient->BInQueueForMatchGroup(k_eTFMatchGroup_Casual_Default);

		// If the player is not in queue for a casual match, queue for a casual match.
		if (!bInQueueForMatchGroup)
		{
			I::TFPartyClient->LoadSavedCasualCriteria();
			I::TFPartyClient->RequestQueueForMatch(k_eTFMatchGroup_Casual_Default);
		}
	}

	// Auto accept.
	if (Vars::Misc::AntiVAC.Value)
	{
		// Get the fps_max, host_timescale, and sv_cheats convars.
		static auto fps_max = g_ConVars.FindVar("fps_max");
		static auto host_timescale = g_ConVars.FindVar("host_timescale");
		static auto sv_cheats = g_ConVars.FindVar("sv_cheats");

		// Keep track of whether the player was connected last frame.
		static bool lastConnect = false;

		// If the player has match invites and is not connected to a server, set sv_cheats to 1, fps_max to 5, and host_timescale to 20.
		if (I::TFGCClientSystem->GetNumMatchInvites() > 0 && !I::EngineClient->IsConnected())
		{
			sv_cheats->SetValue(1);
			fps_max->SetValue(5);
			host_timescale->SetValue(20);
		}
		// If the player has a live match and is not connected to a server, and was connected last frame, join the match.
		else if (I::TFGCClientSystem->BHaveLiveMatch() && !I::EngineClient->IsConnected() && lastConnect)
		{
			I::TFGCClientSystem->JoinMMMatch();
		}
		// If the player is connected to a server and was not connected last frame, set fps_max to 0 and host_timescale to 1.
		else if (I::EngineClient->IsConnected() && !lastConnect)
		{
			fps_max->SetValue(0);
			host_timescale->SetValue(1);
		}

		// Update the value of lastConnect.
		lastConnect = I::EngineClient->IsConnected();
	}

	// Join message spam.
	if (Vars::Misc::JoinSpam.Value)
	{
		// Create a timer to spam the join match command.
		static Timer spamTimer{ };

		// If the timer has expired, join the match.
		if (spamTimer.Run(200))
		{
			I::TFGCClientSystem->JoinMMMatch();
		}
	}
}
