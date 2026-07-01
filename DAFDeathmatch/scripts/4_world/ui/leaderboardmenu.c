class DAFDMLeaderboardCache
{
	private static ref TStringArray s_Names;
	private static ref array<int> s_Kills;
	private static ref array<int> s_Deaths;
	private static ref array<ref DAFDMLeaderboardMenu> s_Menus;

	static void Ensure()
	{
		if (!s_Names)
			s_Names = new TStringArray();

		if (!s_Kills)
			s_Kills = new array<int>();

		if (!s_Deaths)
			s_Deaths = new array<int>();

		if (!s_Menus)
			s_Menus = new array<ref DAFDMLeaderboardMenu>();
	}

	static void SetSnapshot(TStringArray data)
	{
		Ensure();
		s_Names.Clear();
		s_Kills.Clear();
		s_Deaths.Clear();

		if (data)
		{
			for (int i = 0; i + 2 < data.Count(); i += 3)
			{
				s_Names.Insert(data.Get(i));
				s_Kills.Insert(data.Get(i + 1).ToInt());
				s_Deaths.Insert(data.Get(i + 2).ToInt());
			}
		}

		for (int menuIndex = s_Menus.Count() - 1; menuIndex >= 0; menuIndex--)
		{
			DAFDMLeaderboardMenu menu = s_Menus.Get(menuIndex);
			if (menu)
				menu.Refresh();
			else
				s_Menus.Remove(menuIndex);
		}
	}

	static int Count()
	{
		Ensure();
		return s_Names.Count();
	}

	static string GetName(int index)
	{
		Ensure();
		if (index < 0 || index >= s_Names.Count())
			return "";

		return s_Names.Get(index);
	}

	static int GetKills(int index)
	{
		Ensure();
		if (index < 0 || index >= s_Kills.Count())
			return 0;

		return s_Kills.Get(index);
	}

	static int GetDeaths(int index)
	{
		Ensure();
		if (index < 0 || index >= s_Deaths.Count())
			return 0;

		return s_Deaths.Get(index);
	}

	static void Register(DAFDMLeaderboardMenu menu)
	{
		if (!menu)
			return;

		Ensure();
		if (s_Menus.Find(menu) < 0)
			s_Menus.Insert(menu);
	}

	static void Unregister(DAFDMLeaderboardMenu menu)
	{
		if (!s_Menus || !menu)
			return;

		int index = s_Menus.Find(menu);
		if (index >= 0)
			s_Menus.Remove(index);
	}
}

class DAFDMLeaderboardMenu
{
	private static const int MAX_ROWS = 10;
	private Widget m_Root;
	private TextWidget m_EmptyText;
	private ref array<Widget> m_Rows;
	private ref array<TextWidget> m_RankLabels;
	private ref array<TextWidget> m_NameLabels;
	private ref array<TextWidget> m_KillLabels;
	private ref array<TextWidget> m_DeathLabels;

	void DAFDMLeaderboardMenu(Widget parent)
	{
		m_Rows = new array<Widget>();
		m_RankLabels = new array<TextWidget>();
		m_NameLabels = new array<TextWidget>();
		m_KillLabels = new array<TextWidget>();
		m_DeathLabels = new array<TextWidget>();

		m_Root = GetGame().GetWorkspace().CreateWidgets("DAFDeathmatch/assets/leaderboard.layout", parent);
		if (!m_Root)
		{
			Print("DAFDeathmatch: failed to create leaderboard menu widget");
			return;
		}

		m_EmptyText = TextWidget.Cast(m_Root.FindAnyWidget("EmptyText"));
		for (int i = 0; i < MAX_ROWS; i++)
		{
			string prefix = "Row" + i.ToString();
			m_Rows.Insert(m_Root.FindAnyWidget(prefix));
			m_RankLabels.Insert(TextWidget.Cast(m_Root.FindAnyWidget(prefix + "Rank")));
			m_NameLabels.Insert(TextWidget.Cast(m_Root.FindAnyWidget(prefix + "Name")));
			m_KillLabels.Insert(TextWidget.Cast(m_Root.FindAnyWidget(prefix + "Kills")));
			m_DeathLabels.Insert(TextWidget.Cast(m_Root.FindAnyWidget(prefix + "Deaths")));
		}

		DAFDMLeaderboardCache.Register(this);
		Refresh();
	}

	void Refresh()
	{
		int count = DAFDMLeaderboardCache.Count();
		if (m_EmptyText)
			m_EmptyText.Show(count == 0);

		for (int i = 0; i < MAX_ROWS; i++)
		{
			bool showRow = i < count;
			Widget row = m_Rows.Get(i);
			if (row)
				row.Show(showRow);

			if (!showRow)
				continue;

			TextWidget rankLabel = m_RankLabels.Get(i);
			TextWidget nameLabel = m_NameLabels.Get(i);
			TextWidget killLabel = m_KillLabels.Get(i);
			TextWidget deathLabel = m_DeathLabels.Get(i);

			if (rankLabel)
				rankLabel.SetText((i + 1).ToString());

			if (nameLabel)
				nameLabel.SetText(FormatName(DAFDMLeaderboardCache.GetName(i)));

			if (killLabel)
				killLabel.SetText(DAFDMLeaderboardCache.GetKills(i).ToString());

			if (deathLabel)
				deathLabel.SetText(DAFDMLeaderboardCache.GetDeaths(i).ToString());
		}
	}

	string FormatName(string name)
	{
		if (name == "")
			return "Unknown";

		if (name.Length() > 28)
			return name.Substring(0, 25) + "...";

		return name;
	}

	void Destroy()
	{
		DAFDMLeaderboardCache.Unregister(this);

		if (m_Root)
		{
			m_Root.Unlink();
			m_Root = null;
		}
	}
}
