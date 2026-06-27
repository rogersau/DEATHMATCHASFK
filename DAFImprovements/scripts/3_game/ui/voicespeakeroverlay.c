class DAFVoiceSpeakerOverlay
{
	private Widget m_Root;
	private TextWidget m_Text;
	private ref map<string, string> m_Speakers;

	void DAFVoiceSpeakerOverlay()
	{
		m_Root = GetGame().GetWorkspace().CreateWidgets("DAFImprovements/assets/voicespeakers.layout", null);
		if (m_Root)
			m_Text = TextWidget.Cast(m_Root.FindAnyWidget("VoiceSpeakersText"));
		else
			Print("DAFImprovements: failed to create voice speaker HUD widget");

		m_Speakers = new map<string, string>();
		UpdateText();
	}

	void ~DAFVoiceSpeakerOverlay()
	{
		if (m_Root)
			m_Root.Unlink();
	}

	void AddSpeaker(string id, string name)
	{
		if (id == "")
			id = name;

		if (name == "")
			name = "Unknown";

		m_Speakers.Set(id, name);
		UpdateText();
	}

	void RemoveSpeaker(string id, string name = "")
	{
		if (id == "" && name != "")
			id = name;

		if (m_Speakers.Contains(id))
			m_Speakers.Remove(id);
		else if (name != "")
		{
			for (int i = m_Speakers.Count() - 1; i >= 0; i--)
			{
				if (m_Speakers.GetElement(i) == name)
					m_Speakers.Remove(m_Speakers.GetKey(i));
			}
		}

		UpdateText();
	}

	private void UpdateText()
	{
		if (!m_Text || !m_Root)
			return;

		if (m_Speakers.Count() == 0)
		{
			m_Text.SetText("");
			m_Root.Show(false);
			return;
		}

		string text = "VOICE";
		for (int i = 0; i < m_Speakers.Count(); i++)
		{
			text = text + "\n" + m_Speakers.GetElement(i);
		}

		m_Text.SetText(text);
		m_Root.Show(true);
	}
}
