class KillFeedWrapper
{
	private Widget root;
	private Widget playerCountRoot;
	private Widget roundTimerRoot;
	private TextWidget playerCount;
	private TextWidget killsCount;
	private TextWidget deathsCount;
	private TextWidget seasonStats;
	private TextWidget roundTimer;
	private string roundLabel;
	private int playerCountValue;
	private string playerCountStatus;
	private int killsValue;
	private int deathsValue;
	private int seasonPointsValue;
	private int seasonRankValue;
	private ref array<ref KillFeedItem> items;

	void KillFeedWrapper()
	{
		root = GetGame().GetWorkspace().CreateWidgets("DAFDeathmatch/assets/feedwrapper.layout", null);
		playerCountRoot = GetGame().GetWorkspace().CreateWidgets("DAFDeathmatch/assets/playercount.layout", null);
		roundTimerRoot = GetGame().GetWorkspace().CreateWidgets("DAFDeathmatch/assets/roundtimer.layout", null);

		if (playerCountRoot)
		{
			playerCount = TextWidget.Cast(playerCountRoot.FindAnyWidget("PlayerCount"));
			killsCount = TextWidget.Cast(playerCountRoot.FindAnyWidget("KillsCount"));
			deathsCount = TextWidget.Cast(playerCountRoot.FindAnyWidget("DeathsCount"));
			seasonStats = TextWidget.Cast(playerCountRoot.FindAnyWidget("SeasonStats"));
		}
		else
			Print("DAFDeathmatch: failed to create player count HUD widget");

		if (roundTimerRoot)
			roundTimer = TextWidget.Cast(roundTimerRoot.FindAnyWidget("RoundTimer"));
		else
			Print("DAFDeathmatch: failed to create round timer HUD widget");

		items = new array<ref KillFeedItem>();
		roundLabel = "Round";
		playerCountValue = 0;
		playerCountStatus = "";
		killsValue = 0;
		deathsValue = 0;
		seasonPointsValue = 0;
		seasonRankValue = 0;
		SetRoundTimeRemaining(-1);
		SetRoundStats(0, 0, 0, 0);
	}

	Widget GetRoot()
	{
		return root;
	}

	void AddItem(Param7<string, string, string, int, string, int, int> data)
	{
		// Insert new item at the end
		items.Insert(new KillFeedItem(this, data.param1, data.param2, data.param3, data.param4, data.param5, data.param6, data.param7));

		// Enforce maximum of 5 items. If we now have more than 5, destroy the oldest (index 0).
		int maxItems = 5;
		if (items.Count() > maxItems)
		{
			KillFeedItem oldest = items.Get(0);
			// Call Destroy on the oldest so it cleans up its widget and weapon
			if (oldest) oldest.Destroy();
			// Remove it from the array if still present
			int idx = items.Find(oldest);
			if (idx >= 0) items.Remove(idx);
		}
	}

	void SetPlayerCount(int count)
	{
		playerCountValue = count;
		UpdatePlayerCount();
	}

	void SetPlayerCountStatus(int count, string status)
	{
		playerCountValue = count;
		playerCountStatus = status;
		UpdatePlayerCount();
	}

	void UpdatePlayerCount()
	{
		if (!playerCount)
			return;

		string text = "Players: " + playerCountValue.ToString();
		if (playerCountStatus != "")
			text += " - " + playerCountStatus;

		if (playerCount)
			playerCount.SetText(text);
	}

	void SetRoundStats(int kills, int deaths, int seasonPoints = 0, int seasonRank = 0)
	{
		killsValue = kills;
		deathsValue = deaths;
		seasonPointsValue = seasonPoints;
		seasonRankValue = seasonRank;
		UpdateRoundStats();
	}

	void UpdateRoundStats()
	{
		if (killsCount)
			killsCount.SetText(killsValue.ToString());

		if (deathsCount)
			deathsCount.SetText(deathsValue.ToString());

		if (seasonStats)
		{
			string rankText = "-";
			if (seasonRankValue > 0)
				rankText = "#" + seasonRankValue.ToString();

			seasonStats.SetText("Season: " + seasonPointsValue.ToString() + " pts | " + rankText);
		}
	}

	void SetRoundTimeRemaining(int seconds)
	{
		if (!roundTimerRoot || !roundTimer)
			return;

		if (seconds < 0)
		{
			roundTimer.SetText("");
			roundTimerRoot.Show(false);
			return;
		}

		int minutes = seconds / 60;
		int remainder = seconds % 60;
		string remainderText = remainder.ToString();
		if (remainder < 10)
			remainderText = "0" + remainderText;

		roundTimer.SetText(roundLabel + " " + minutes.ToString() + ":" + remainderText);
		roundTimerRoot.Show(true);
	}

	void SetRoundLabel(string label)
	{
		if (label == "")
			roundLabel = "Round";
		else
			roundLabel = label;
	}

	void ClearItems()
	{
		for (int i = items.Count() - 1; i >= 0; i--)
		{
			KillFeedItem item = items.Get(i);
			if (item)
				item.Destroy();
		}

		items.Clear();
	}

	void Destroy()
	{
		ClearItems();

		if (playerCountRoot)
		{
			playerCountRoot.Unlink();
			playerCountRoot = null;
		}

		if (roundTimerRoot)
		{
			roundTimerRoot.Unlink();
			roundTimerRoot = null;
		}

		if (root)
		{
			root.Unlink();
			root = null;
		}
	}

	void RemoveItem(KillFeedItem item)
	{
		int idx = items.Find(item);
		// ensure idx is valid (>= 0) before removing
		if (idx >= 0)
		{
			items.Remove(idx);
		}
	}

}
