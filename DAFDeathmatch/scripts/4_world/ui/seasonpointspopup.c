class DAFSeasonPointsPopup
{
	private static const int MAX_VISIBLE = 3;
	private static const float BASE_Y = -132;
	private static const float STACK_GAP = 42;
	private static ref array<ref DAFSeasonPointsPopup> s_Current;

	private Widget m_Root;
	private TextWidget m_Text;
	private TextWidget m_ShadowLeft;
	private TextWidget m_ShadowRight;
	private TextWidget m_ShadowTop;
	private TextWidget m_ShadowBottom;

	void DAFSeasonPointsPopup(string text)
	{
		m_Root = GetGame().GetWorkspace().CreateWidgets("DAFDeathmatch/assets/seasonpointspopup.layout", null);
		if (!m_Root)
		{
			Print("DAFDeathmatch: failed to create season points popup widget");
			return;
		}

		m_Text = TextWidget.Cast(m_Root.FindAnyWidget("SeasonPointsText"));
		m_ShadowLeft = TextWidget.Cast(m_Root.FindAnyWidget("SeasonPointsShadowLeft"));
		m_ShadowRight = TextWidget.Cast(m_Root.FindAnyWidget("SeasonPointsShadowRight"));
		m_ShadowTop = TextWidget.Cast(m_Root.FindAnyWidget("SeasonPointsShadowTop"));
		m_ShadowBottom = TextWidget.Cast(m_Root.FindAnyWidget("SeasonPointsShadowBottom"));
		SetText(text);

		GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.Destroy, 1100, false);
	}

	static void Show(string text)
	{
		if (text == "")
			return;

		EnsureCurrent();
		while (s_Current.Count() >= MAX_VISIBLE)
		{
			DAFSeasonPointsPopup oldest = s_Current[0];
			if (oldest)
				oldest.Destroy();
			else
				s_Current.Remove(0);
		}

		s_Current.Insert(new DAFSeasonPointsPopup(text));
		Reflow();
	}

	void Destroy()
	{
		if (s_Current)
		{
			int index = s_Current.Find(this);
			if (index >= 0)
				s_Current.Remove(index);
		}

		if (m_Root)
		{
			m_Root.Unlink();
			m_Root = null;
		}

		Reflow();
	}

	private void SetText(string text)
	{
		if (m_Text)
			m_Text.SetText(text);
		if (m_ShadowLeft)
			m_ShadowLeft.SetText(text);
		if (m_ShadowRight)
			m_ShadowRight.SetText(text);
		if (m_ShadowTop)
			m_ShadowTop.SetText(text);
		if (m_ShadowBottom)
			m_ShadowBottom.SetText(text);
	}

	private void SetStackIndex(int index)
	{
		if (m_Root)
			m_Root.SetPos(0, BASE_Y - (STACK_GAP * index));
	}

	private static void EnsureCurrent()
	{
		if (!s_Current)
			s_Current = new array<ref DAFSeasonPointsPopup>();
	}

	private static void Reflow()
	{
		if (!s_Current)
			return;

		for (int i = 0; i < s_Current.Count(); i++)
		{
			DAFSeasonPointsPopup popup = s_Current[i];
			if (popup)
				popup.SetStackIndex(s_Current.Count() - 1 - i);
		}
	}
}
