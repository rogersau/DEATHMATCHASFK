class DAFServerKill
{
	string KillerName;
	string VictimName;
	string WeaponName;
	int Distance;
	bool Headshot;

	void DAFServerKill(string killerName, string victimName, string weaponName, int distance, bool headshot)
	{
		KillerName = killerName;
		VictimName = victimName;
		WeaponName = weaponName;
		Distance = distance;
		Headshot = headshot;
	}
}

class DAFServerDiscordStats
{
	private static ref array<ref DAFServerKill> Kills = new array<ref DAFServerKill>();
	private static ref TStringIntMap WeaponKills = new TStringIntMap();
	private static int FurthestDistance;
	private static string FurthestKiller;
	private static string FurthestWeapon;

	static void Reset()
	{
		Kills.Clear();
		WeaponKills.Clear();
		FurthestDistance = 0;
		FurthestKiller = "";
		FurthestWeapon = "";
	}

	static void RegisterKill(string killerName, string victimName, string weaponName, int distance, bool headshot)
	{
		Kills.Insert(new DAFServerKill(killerName, victimName, weaponName, distance, headshot));
		if (Kills.Count() > 20)
			Kills.Remove(0);

		if (weaponName == "")
			weaponName = "Unknown";

		if (!WeaponKills.Contains(weaponName))
			WeaponKills.Set(weaponName, 0);
		WeaponKills.Set(weaponName, WeaponKills.Get(weaponName) + 1);

		if (distance > FurthestDistance)
		{
			FurthestDistance = distance;
			FurthestKiller = killerName;
			FurthestWeapon = weaponName;
		}
	}

	static string EnhanceKillMessage(string message)
	{
		foreach (DAFServerKill kill : Kills)
		{
			if (!kill || !kill.Headshot)
				continue;

			string needle = kill.KillerName + " killed " + kill.VictimName;
			if (message.IndexOf(needle) >= 0 && message.IndexOf("HEADSHOT") < 0)
				return message + " [HEADSHOT]";
		}

		return message;
	}

	static string GetRoundSummary()
	{
		string summary = "";
		string highlights = GetRoundHighlights();

		if (highlights != "")
			summary += "\\n**Round Highlights**\\n" + highlights;

		return summary;
	}

	static string GetRoundHighlights()
	{
		string summary = "";

		string bestWeapon;
		int bestKills;
		GetTopWeapon(bestWeapon, bestKills);

		if (FurthestDistance > 0)
		{
			summary += string.Format(
				"- Furthest shot: **%1m** by **%2** using `%3`\\n",
				FurthestDistance,
				FurthestKiller,
				FurthestWeapon);
		}

		if (bestKills > 0)
			summary += string.Format("- Top weapon: **%1** with **%2** kills\\n", bestWeapon, bestKills);

		return summary;
	}

	static void GetTopWeapon(out string bestWeapon, out int bestKills)
	{
		bestWeapon = "";
		bestKills = 0;

		for (int i = 0; i < WeaponKills.Count(); i++)
		{
			string weaponName = WeaponKills.GetKey(i);
			int kills = WeaponKills.GetElement(i);
			if (kills > bestKills)
			{
				bestWeapon = weaponName;
				bestKills = kills;
			}
		}
	}

	static string FormatKD(int kills, int deaths)
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
}

modded class DMDiscordWebhook
{
	override void PostMessage(string message)
	{
		super.PostMessage(DAFServerDiscordStats.EnhanceKillMessage(message));
	}

	override void PostLeaderboard(TDMScoreArray leaderboard)
	{
		PostLeaderboardEmbed(leaderboard);
	}

	string DAF_EscapeJson(string value)
	{
		value = DAF_Replace(value, "\"", "'");
		value = DAF_Replace(value, "\n", "\\n");
		value = DAF_Replace(value, "\r", "");
		return value;
	}

	string DAF_Replace(string value, string needle, string replacement)
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

	void PostJson(string payload)
	{
		DMDiscordWebhookCallback cbx = new DMDiscordWebhookCallback();
		RestContext ctx = GetRestApi().GetRestContext(m_url);
		ctx.SetHeader("application/json");
		ctx.POST(cbx, "", payload);
	}

	void PostLeaderboardEmbed(TDMScoreArray leaderboard)
	{
		string leaderboardText = "";

		int limit = leaderboard.Count();
		if (limit > 10)
			limit = 10;

		for (int i = 0; i < limit; i++)
		{
			DMScore score = leaderboard.Get(i);
			leaderboardText += string.Format(
				"**%1. %2** — `%3 K` / `%4 D` — `%5 K/D`\\n",
				i + 1,
				score.m_name,
				score.m_kills,
				score.m_deaths,
				DAFServerDiscordStats.FormatKD(score.m_kills, score.m_deaths));
		}

		if (leaderboard.Count() > limit)
			leaderboardText += string.Format("_and %1 more players_\\n", leaderboard.Count() - limit);

		if (leaderboardText == "")
			leaderboardText = "_No players scored this round._";

		string highlights = DAFServerDiscordStats.GetRoundHighlights();
		if (highlights == "")
			highlights = "_No round highlights recorded._";

		string q = "\"";
		string payload = "{";
		payload += q + "content" + q + ":" + q + q + ",";
		payload += q + "embeds" + q + ":[{";
		payload += q + "title" + q + ":" + q + "Round Leaderboard" + q + ",";
		payload += q + "color" + q + ":16753920,";
		payload += q + "description" + q + ":" + q + DAF_EscapeJson(leaderboardText) + q + ",";
		payload += q + "fields" + q + ":[{";
		payload += q + "name" + q + ":" + q + "Round Highlights" + q + ",";
		payload += q + "value" + q + ":" + q + DAF_EscapeJson(highlights) + q + ",";
		payload += q + "inline" + q + ":false";
		payload += "}],";
		payload += q + "footer" + q + ":{" + q + "text" + q + ":" + q + "Top " + limit.ToString() + " players shown" + q + "}";
		payload += "}]}";

		PostJson(payload);
	}
}
