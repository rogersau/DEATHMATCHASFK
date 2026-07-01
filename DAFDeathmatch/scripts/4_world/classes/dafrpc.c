/**
 * DAFRPC owns the server-to-client RPC seam shared across the DAF addons.
 *
 * The standalone deathmatch client and server both need to send and receive
 * the same signals. Before this module, the RPC ids were hand-coordinated magic numbers (-74700005
 * through -74700012) scattered as raw literals across five files. ADR-0001
 * mandated a local wrapper to pay that down; this is it.
 *
 * The seam is deliberately shaped so Community Framework RPC can replace the
 * id allocation and the broadcast mechanism behind it without touching callers:
 *
 *   - The named id constants are the single source of truth for the wire ids.
 *     Under CF these become ModRPC lookups instead of integer ids.
 *   - The typed Send* helpers are the only server-facing surface. Callers never
 *     build a Param or mention an id. Each helper owns its own player iteration,
 *     collapsing the duplicated foreach-loops that lived at every old send site.
 *   - OnRPCReceived is the only client-facing surface. It centralizes the read
 *     and dispatch that previously lived inline in PlayerBase.OnRPC.
 *
 * See docs/adr/0002-adopt-community-framework-rpc.md.
 */
class DAFRPC
{
	// Wire ids. Negative range avoids collisions with vanilla and most mods.
	// Do not reference these outside this module.
	static const int RPC_KILLFEED_ITEM        = -74700005; // Param7<string, string, string, int, string, int, int>
	static const int RPC_PLAYER_COUNT         = -74700006; // Param1<int>
	static const int RPC_RESPAWN_CURSOR_FIX   = -74700007; // Param1<bool>
	static const int RPC_KILLFEED_RESET       = -74700008; // Param1<bool>
	static const int RPC_ROUND_TIME_SECONDS   = -74700009; // Param1<int>
	static const int RPC_ROUND_LABEL          = -74700010; // Param1<string>
	static const int RPC_PLAYER_COUNT_STATUS  = -74700011; // Param2<int, string>
	static const int RPC_ROUND_HUD_STATE      = -74700012; // Param5<int, string, int, string, bool>
	static const int RPC_ROUND_STATS          = -74700013; // Param4<int, int, int, int>
	static const int RPC_SEASON_POINTS_POPUP  = -74700014; // Param1<string>

	// ---------------------------------------------------------------------------
	// Server -> client send helpers. Only call these on the server.
	// Each one resolves its own recipient list; callers pass the payload only.
	// ---------------------------------------------------------------------------

	/** Push a killfeed entry to every connected player. */
	static void SendKillfeedItem(string murderName, string targetName, string murderWeaponData, int distance, string message, int type, int headshot)
	{
		array<Man> players = new array<Man>();
		GetGame().GetWorld().GetPlayerList(players);

		foreach (Man man: players)
		{
			PlayerBase recipient = PlayerBase.Cast(man);
			if (recipient && recipient.GetIdentity())
				recipient.RPCSingleParam(RPC_KILLFEED_ITEM, new Param7<string, string, string, int, string, int, int>(murderName, targetName, murderWeaponData, distance, message, type, headshot), true, recipient.GetIdentity());
		}
	}

	/** Push the current player count to every connected player. */
	static void SendPlayerCount(int count)
	{
		array<Man> players = new array<Man>();
		GetGame().GetWorld().GetPlayerList(players);

		foreach (Man man: players)
		{
			PlayerBase recipient = PlayerBase.Cast(man);
			if (recipient && recipient.GetIdentity())
				recipient.RPCSingleParam(RPC_PLAYER_COUNT, new Param1<int>(count), true, recipient.GetIdentity());
		}
	}

	/** Push a player's own round K/D stats to that player. Pass season values only when the caller owns them. */
	static void SendRoundStats(PlayerIdentity identity, int kills, int deaths, int seasonPoints = -1, int seasonRank = -1)
	{
		if (!identity)
			return;

		PlayerBase recipient = PlayerBase.Cast(identity.GetPlayer());
		if (recipient)
			recipient.RPCSingleParam(RPC_ROUND_STATS, new Param4<int, int, int, int>(kills, deaths, seasonPoints, seasonRank), true, identity);
	}

	/** Show a small client-side season points popup for the recipient only. */
	static void SendSeasonPointsPopup(PlayerIdentity identity, string text)
	{
		if (!identity || text == "")
			return;

		PlayerBase recipient = PlayerBase.Cast(identity.GetPlayer());
		if (recipient)
			recipient.RPCSingleParam(RPC_SEASON_POINTS_POPUP, new Param1<string>(text), true, identity);
	}

	/** Push zeroed round K/D stats to every connected player. */
	static void ResetRoundStatsHud()
	{
		array<Man> players = new array<Man>();
		GetGame().GetWorld().GetPlayerList(players);

		foreach (Man man: players)
		{
			PlayerBase recipient = PlayerBase.Cast(man);
			if (recipient && recipient.GetIdentity())
				SendRoundStats(recipient.GetIdentity(), 0, 0);
		}
	}

	/** Send the respawn cursor-repair nudge to a single reconnecting player. */
	static void SendRespawnCursorFix(PlayerIdentity identity)
	{
		if (!identity)
			return;

		PlayerBase recipient = PlayerBase.Cast(identity.GetPlayer());
		if (recipient)
			recipient.RPCSingleParam(RPC_RESPAWN_CURSOR_FIX, new Param1<bool>(true), true, identity);
	}

	/** Tell every client to clear its killfeed (round reset). */
	static void SendKillfeedReset()
	{
		array<Man> players = new array<Man>();
		GetGame().GetWorld().GetPlayerList(players);

		foreach (Man man: players)
		{
			PlayerBase recipient = PlayerBase.Cast(man);
			if (recipient && recipient.GetIdentity())
				recipient.RPCSingleParam(RPC_KILLFEED_RESET, new Param1<bool>(true), true, recipient.GetIdentity());
		}
	}

	/** Broadcast the round time remaining (seconds) to every connected player. */
	static void SendRoundTimeSeconds(int seconds)
	{
		array<Man> players = new array<Man>();
		GetGame().GetWorld().GetPlayerList(players);

		foreach (Man man: players)
		{
			PlayerBase recipient = PlayerBase.Cast(man);
			if (recipient && recipient.GetIdentity())
				recipient.RPCSingleParam(RPC_ROUND_TIME_SECONDS, new Param1<int>(seconds), true, recipient.GetIdentity());
		}
	}

	/** Broadcast the round display label to every connected player. */
	static void SendRoundLabel(string label)
	{
		array<Man> players = new array<Man>();
		GetGame().GetWorld().GetPlayerList(players);

		foreach (Man man: players)
		{
			PlayerBase recipient = PlayerBase.Cast(man);
			if (recipient && recipient.GetIdentity())
				recipient.RPCSingleParam(RPC_ROUND_LABEL, new Param1<string>(label), true, recipient.GetIdentity());
		}
	}

	/** Push the player count plus a status string (e.g. warmup indicator) to every client. */
	static void SendPlayerCountStatus(int count, string status)
	{
		array<Man> players = new array<Man>();
		GetGame().GetWorld().GetPlayerList(players);

		foreach (Man man: players)
		{
			PlayerBase recipient = PlayerBase.Cast(man);
			if (recipient && recipient.GetIdentity())
				recipient.RPCSingleParam(RPC_PLAYER_COUNT_STATUS, new Param2<int, string>(count, status), true, recipient.GetIdentity());
		}
	}

	/** Push the full round HUD snapshot to a single reconnecting player. */
	static void SendRoundHudState(PlayerIdentity identity, int roundSeconds, string roundLabel, int playerCount, string status, bool manualRespawnAllowed)
	{
		if (!identity)
			return;

		PlayerBase recipient = PlayerBase.Cast(identity.GetPlayer());
		if (recipient)
			recipient.RPCSingleParam(RPC_ROUND_HUD_STATE, new Param5<int, string, int, string, bool>(roundSeconds, roundLabel, playerCount, status, manualRespawnAllowed), true, identity);
	}

	/** Push the full round HUD snapshot to every connected player. */
	static void BroadcastRoundHudState(int roundSeconds, string roundLabel, int playerCount, string status, bool manualRespawnAllowed)
	{
		array<Man> players = new array<Man>();
		GetGame().GetWorld().GetPlayerList(players);

		foreach (Man man: players)
		{
			PlayerBase recipient = PlayerBase.Cast(man);
			if (recipient && recipient.GetIdentity())
				recipient.RPCSingleParam(RPC_ROUND_HUD_STATE, new Param5<int, string, int, string, bool>(roundSeconds, roundLabel, playerCount, status, manualRespawnAllowed), true, recipient.GetIdentity());
		}
	}

	/** Push the round time remaining to a single reconnecting player. */
	static void SendRoundTimeSecondsTo(PlayerIdentity identity, int seconds)
	{
		if (!identity)
			return;

		PlayerBase recipient = PlayerBase.Cast(identity.GetPlayer());
		if (recipient)
			recipient.RPCSingleParam(RPC_ROUND_TIME_SECONDS, new Param1<int>(seconds), true, identity);
	}

	/** Push the round label to a single reconnecting player. */
	static void SendRoundLabelTo(PlayerIdentity identity, string label)
	{
		if (!identity)
			return;

		PlayerBase recipient = PlayerBase.Cast(identity.GetPlayer());
		if (recipient)
			recipient.RPCSingleParam(RPC_ROUND_LABEL, new Param1<string>(label), true, identity);
	}

	/** Push the player count plus status to a single reconnecting player. */
	static void SendPlayerCountStatusTo(PlayerIdentity identity, int count, string status)
	{
		if (!identity)
			return;

		PlayerBase recipient = PlayerBase.Cast(identity.GetPlayer());
		if (recipient)
			recipient.RPCSingleParam(RPC_PLAYER_COUNT_STATUS, new Param2<int, string>(count, status), true, identity);
	}
}
