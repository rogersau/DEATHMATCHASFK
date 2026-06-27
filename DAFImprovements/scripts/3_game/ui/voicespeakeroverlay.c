class DAFVoiceSpeakerOverlay
{
	private Widget m_Root;
	private TextWidget m_Text;
	private ref map<string, string> m_Speakers;

	void DAFVoiceSpeakerOverlay()
	{
		m_Root = GetGame().GetWorkspace().CreateWidgets("DAFImprovements/assets/VoiceSpeakers.layout", null);
		m_Text = TextWidget.Cast(m_Root.FindAnyWidget("VoiceSpeakersText"));
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

	void RemoveSpeaker(string id)
	{
		if (m_Speakers.Contains(id))
			m_Speakers.Remove(id);

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
