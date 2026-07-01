class DAFSeasonPointsPopup
{
	private static ref DAFSeasonPointsPopup s_Current;

	private Widget m_Root;
	private TextWidget m_Text;

	void DAFSeasonPointsPopup(string text)
	{
		m_Root = GetGame().GetWorkspace().CreateWidgets("DAFImprovements/assets/seasonpointspopup.layout", null);
		if (!m_Root)
		{
			Print("DAFImprovements: failed to create season points popup widget");
			return;
		}

		m_Text = TextWidget.Cast(m_Root.FindAnyWidget("SeasonPointsText"));
		if (m_Text)
			m_Text.SetText(text);

		GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.Destroy, 1250, false);
	}

	static void Show(string text)
	{
		if (text == "")
			return;

		if (s_Current)
			s_Current.Destroy();

		s_Current = new DAFSeasonPointsPopup(text);
	}

	void Destroy()
	{
		if (s_Current == this)
			s_Current = null;

		if (m_Root)
		{
			m_Root.Unlink();
			m_Root = null;
		}
	}
}
