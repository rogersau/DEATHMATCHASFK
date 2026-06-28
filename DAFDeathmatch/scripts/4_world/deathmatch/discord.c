class DAFDMDiscordKill
{
	string killerName;
	string victimName;
	string weaponName;
	int distance;
	bool headshot;

	void DAFDMDiscordKill(string killer, string victim, string weapon, int dist, bool wasHeadshot)
	{
		killerName = killer;
		victimName = victim;
		weaponName = weapon;
		distance = dist;
		headshot = wasHeadshot;
	}
}

class DAFDMDiscordRoundStats
{
	private ref array<ref DAFDMDiscordKill> m_Kills = new array<ref DAFDMDiscordKill>();
	private ref map<string, int> m_WeaponKills = new map<string, int>();
	private int m_FurthestDistance = 0;
	private string m_FurthestKiller = "";
	private string m_FurthestWeapon = "";

	void Reset()
	{
		m_Kills.Clear();
		m_WeaponKills.Clear();
		m_FurthestDistance = 0;
		m_FurthestKiller = "";
		m_FurthestWeapon = "";
	}

	void RegisterKill(string killerName, string victimName, string weaponName, int distance, bool headshot)
	{
		if (killerName == "")
			killerName = "Survivor";

		if (victimName == "")
			victimName = "Survivor";

		if (weaponName == "")
			weaponName = "Unknown";

		m_Kills.Insert(new DAFDMDiscordKill(killerName, victimName, weaponName, distance, headshot));
		if (m_Kills.Count() > 20)
			m_Kills.Remove(0);

		int current = 0;
		m_WeaponKills.Find(weaponName, current);
		m_WeaponKills.Set(weaponName, current + 1);

		if (distance > m_FurthestDistance)
		{
			m_FurthestDistance = distance;
			m_FurthestKiller = killerName;
			m_FurthestWeapon = weaponName;
		}
	}

	bool HasKills()
	{
		return m_Kills.Count() > 0;
	}

	void GetFurthestShot(out int distance, out string killer, out string weapon)
	{
		distance = m_FurthestDistance;
		killer = m_FurthestKiller;
		weapon = m_FurthestWeapon;
	}

	void GetTopWeapon(out string weaponName, out int kills)
	{
		weaponName = "";
		kills = 0;

		for (int i = 0; i < m_WeaponKills.Count(); i++)
		{
			int count = m_WeaponKills.GetElement(i);
			if (count > kills)
			{
				kills = count;
				weaponName = m_WeaponKills.GetKey(i);
			}
		}
	}
}

class DAFDMDiscordCallback : RestCallback
{
	override void OnError(int errorCode)
	{
		if (errorCode == 8)
		{
			Print("DAFDeathmatch: Discord webhook callback received non-fatal error code 8");
			return;
		}

		PrintFormat("DAFDeathmatch: Discord webhook callback error code=%1", errorCode);
	}

	override void OnTimeout()
	{
		Print("DAFDeathmatch: Discord webhook callback timed out");
	}

	override void OnSuccess(string data, int dataSize)
	{
	}
}

class DAFDMDiscord
{
	private static const string WEBHOOK_HOST_HINT = "discord.com";
	private static const string LEGACY_WEBHOOK_HOST_HINT = "discordapp.com";
	private static const int MASK_PREFIX_LENGTH = 35;
	private static const int MASK_SUFFIX_LENGTH = 6;

	private ref DAFDMDiscordRoundStats m_RoundStats = new DAFDMDiscordRoundStats();

	bool KillfeedEnabled(DAFDMSettings settings)
	{
		return settings && settings.enableDiscordKillfeed && WebhookReady(settings.discordKillfeedWebhookUrl);
	}

	bool ServerEventsEnabled(DAFDMSettings settings)
	{
		return settings && settings.enableDiscordServerEvents && WebhookReady(settings.discordServerEventsWebhookUrl);
	}

	bool WebhookReady(string url)
	{
		return url != "" && (url.IndexOf(WEBHOOK_HOST_HINT) >= 0 || url.IndexOf(LEGACY_WEBHOOK_HOST_HINT) >= 0);
	}

	void ResetRoundStats()
	{
		m_RoundStats.Reset();
	}

	void RegisterPvpKill(DAFDMSettings settings, PlayerBase victim, PlayerBase killer, EntityAI weapon, bool headshot)
	{
		if (!victim || !killer)
			return;

		PlayerBase realKiller = killer;
		PlayerBase realVictim = victim;

		string killerName = ResolvePlayerName(realKiller);
		string victimName = ResolvePlayerName(realVictim);
		string weaponName = ResolveWeaponName(weapon);
		int distance = vector.Distance(realVictim.GetPosition(), realKiller.GetPosition());
		m_RoundStats.RegisterKill(killerName, victimName, weaponName, distance, headshot);
	}

	void PostKillfeedKill(DAFDMSettings settings, PlayerBase victim, PlayerBase killer, EntityAI weapon, bool headshot)
	{
		if (!KillfeedEnabled(settings) || !victim || !killer)
			return;

		PlayerBase realKiller = killer;
		PlayerBase realVictim = victim;

		string killerName = ResolvePlayerName(realKiller);
		string victimName = ResolvePlayerName(realVictim);
		string weaponName = ResolveWeaponName(weapon);
		int distance = vector.Distance(realVictim.GetPosition(), realKiller.GetPosition());

		string headshotTag = "";
		if (headshot)
			headshotTag = " [HEADSHOT]";

		string message = string.Format("**%1** killed **%2** with `%3` (%4m)%5", killerName, victimName, weaponName, distance, headshotTag);
		PostContent(settings.discordKillfeedWebhookUrl, settings.discordSuppressEmbeds, message);
	}

	void PostServerReady(DAFDMSettings settings)
	{
		if (!ServerEventsEnabled(settings))
			return;

		string serverName = ResolveServerName(settings);
		PostContent(settings.discordServerEventsWebhookUrl, settings.discordSuppressEmbeds, string.Format("**%1** server is ready", serverName));
	}

	void PostRoundStart(DAFDMSettings settings, string roundDisplayName, string arenaName, string gameMode, int roundMinutes)
	{
		if (!ServerEventsEnabled(settings))
			return;

		string serverName = ResolveServerName(settings);
		string modeLabel = "FFA";
		if (gameMode == "tdm")
			modeLabel = "TDM";

		string message = string.Format("**%1** round started: **%2** (%3) in %4 (%5m)", serverName, roundDisplayName, modeLabel, arenaName, roundMinutes);
		PostContent(settings.discordServerEventsWebhookUrl, settings.discordSuppressEmbeds, message);
	}

	void PostRoundEnd(DAFDMSettings settings, DAFDMScoreboard scoreboard, DAFDMSettings settingsForTeams, string roundDisplayName, string gameMode, array<string> teamNames)
	{
		if (!ServerEventsEnabled(settings))
			return;

		string serverName = ResolveServerName(settings);
		string description = BuildRoundSummary(scoreboard, gameMode, teamNames);
		string payload = BuildEmbedPayload(settings.discordSuppressEmbeds, serverName + " round ended: " + roundDisplayName, description, 16753920);
		PostJson(settings.discordServerEventsWebhookUrl, payload);
	}

	string BuildRoundSummary(DAFDMScoreboard scoreboard, string gameMode, array<string> teamNames)
	{
		string summary = "";

		if (scoreboard)
		{
			DAFDMScore topPlayer = scoreboard.GetTopPlayerByKills();
			if (topPlayer && topPlayer.kills > 0)
				summary += string.Format("- Top player: **%1** with **%2** kills / **%3** deaths (K/D %4)\\n", topPlayer.name, topPlayer.kills, topPlayer.deaths, FormatKD(topPlayer.kills, topPlayer.deaths));

			DAFDMScore topKd = scoreboard.GetTopPlayerByKD();
			if (topKd && topKd.kills > 0 && (!topPlayer || topKd.id != topPlayer.id))
				summary += string.Format("- Top K/D: **%1** at **%2** (%3 K / %4 D)\\n", topKd.name, FormatKD(topKd.kills, topKd.deaths), topKd.kills, topKd.deaths);
		}

		int furthestDistance;
		string furthestKiller;
		string furthestWeapon;
		m_RoundStats.GetFurthestShot(furthestDistance, furthestKiller, furthestWeapon);
		if (furthestDistance > 0)
			summary += string.Format("- Furthest shot: **%1m** by **%2** using `%3`\\n", furthestDistance, furthestKiller, furthestWeapon);

		string topWeapon;
		int topWeaponKills;
		m_RoundStats.GetTopWeapon(topWeapon, topWeaponKills);
		if (topWeaponKills > 0)
			summary += string.Format("- Top weapon: **%1** with **%2** kills\\n", topWeapon, topWeaponKills);

		if (gameMode == "tdm" && teamNames && teamNames.Count() >= 2 && scoreboard)
		{
			string teamLines = "";
			foreach (string team: teamNames)
			{
				if (team == "")
					continue;

				if (teamLines != "")
					teamLines += ", ";

				teamLines += string.Format("%1: %2", team, scoreboard.GetTeamScore(team));
			}

			if (teamLines != "")
				summary += "- Team scores: " + teamLines + "\\n";
		}

		if (summary == "")
			summary = "_No round highlights recorded._";

		return summary;
	}

	bool TestEndpoint(DAFDMSettings settings, bool killfeed)
	{
		if (!settings)
		{
			Print("DAFDeathmatch: discord test failed because settings are missing");
			return false;
		}

		bool enabled;
		string url;
		string label;
		if (killfeed)
		{
			enabled = settings.enableDiscordKillfeed;
			url = settings.discordKillfeedWebhookUrl;
			label = "killfeed";
		}
		else
		{
			enabled = settings.enableDiscordServerEvents;
			url = settings.discordServerEventsWebhookUrl;
			label = "events";
		}

		if (!enabled)
		{
			PrintFormat("DAFDeathmatch: discord %1 test skipped: endpoint disabled", label);
			return false;
		}

		if (url == "")
		{
			PrintFormat("DAFDeathmatch: discord %1 test skipped: webhook URL is empty", label);
			return false;
		}

		if (!WebhookReady(url))
		{
			PrintFormat("DAFDeathmatch: discord %1 test skipped: webhook URL does not look like a Discord webhook (%2)", label, MaskWebhookUrl(url));
			return false;
		}

		PostContent(url, settings.discordSuppressEmbeds, string.Format("**%1** discord %2 endpoint test", ResolveServerName(settings), label));
		PrintFormat("DAFDeathmatch: discord %1 test posted to %2", label, MaskWebhookUrl(url));
		return true;
	}

	void PostContent(string url, bool suppressEmbeds, string content)
	{
		if (!WebhookReady(url))
			return;

		PostJson(url, BuildContentPayload(suppressEmbeds, content));
	}

	void PostJson(string url, string payload)
	{
		if (!WebhookReady(url))
			return;

		RestContext ctx = GetRestApi().GetRestContext(url);
		ctx.SetHeader("application/json");
		ctx.POST(new DAFDMDiscordCallback(), "", payload);
	}

	string BuildContentPayload(bool suppressEmbeds, string content)
	{
		string q = "\"";
		string payload = "{" + q + "content" + q + ":" + q + EscapeJson(content) + q;
		if (suppressEmbeds)
			payload += "," + q + "flags" + q + ":4";
		payload += "}";
		return payload;
	}

	string BuildEmbedPayload(bool suppressEmbeds, string title, string description, int color)
	{
		string q = "\"";
		string payload = "{" + q + "content" + q + ":" + q + q + ",";
		payload += q + "embeds" + q + ":[{";
		payload += q + "title" + q + ":" + q + EscapeJson(title) + q + ",";
		payload += q + "color" + q + ":" + color.ToString() + ",";
		payload += q + "description" + q + ":" + q + EscapeJson(description) + q;
		payload += "}]}";
		return payload;
	}

	string EscapeJson(string value)
	{
		int backslashCode = 92;
		string backslash = backslashCode.AsciiToString();
		value = Replace(value, backslash, backslash + backslash);
		value = Replace(value, "\"", "'");
		value = Replace(value, "\n", "\\n");
		value = Replace(value, "\r", "");
		return value;
	}

	string Replace(string value, string needle, string replacement)
	{
		array<string> parts = new array<string>();
		value.Split(needle, parts);
		if (parts.Count() <= 1)
			return value;

		string result = "";
		for (int i = 0; i < parts.Count(); i++)
		{
			if (i > 0)
				result += replacement;

			result += parts[i];
		}

		return result;
	}

	string FormatKD(int kills, int deaths)
	{
		if (deaths <= 0)
			return kills.ToString() + ".00";

		int scaled = (kills * 100) / deaths;
		int whole = scaled / 100;
		int fraction = scaled % 100;
		string fractionText = fraction.ToString();
		if (fraction < 10)
			fractionText = "0" + fractionText;

		return whole.ToString() + "." + fractionText;
	}

	string ResolvePlayerName(PlayerBase player)
	{
		if (!player || !player.GetIdentity())
			return "Survivor";

		return player.GetIdentity().GetName();
	}

	string ResolveWeaponName(EntityAI weapon)
	{
		if (!weapon)
			return "Unknown";

		string type = weapon.GetType();
		if (type == "")
			return "Unknown";

		return type;
	}

	string ResolveServerName(DAFDMSettings settings)
	{
		if (settings && settings.discordServerName != "")
			return settings.discordServerName;

		return "DAF Deathmatch";
	}

	string MaskWebhookUrl(string url)
	{
		if (url == "")
			return "<empty>";

		int length = url.Length();
		if (length <= MASK_PREFIX_LENGTH + MASK_SUFFIX_LENGTH)
			return "<hidden>";

		return url.Substring(0, MASK_PREFIX_LENGTH) + "..." + url.Substring(length - MASK_SUFFIX_LENGTH, MASK_SUFFIX_LENGTH);
	}
}
