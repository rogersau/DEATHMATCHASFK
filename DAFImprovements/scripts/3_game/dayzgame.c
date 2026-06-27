modded class DayZGame
{
	private ref DAFVoiceSpeakerOverlay m_DAFVoiceSpeakerOverlay;

	override void AddVoiceNotification(VONStopSpeakingEventParams vonStartParams)
	{
		super.AddVoiceNotification(vonStartParams);

		if (!m_DAFVoiceSpeakerOverlay)
			m_DAFVoiceSpeakerOverlay = new DAFVoiceSpeakerOverlay();

		m_DAFVoiceSpeakerOverlay.AddSpeaker(vonStartParams.param2, vonStartParams.param1);
	}

	override void RemoveVoiceNotification(VONStopSpeakingEventParams vonStopParams)
	{
		super.RemoveVoiceNotification(vonStopParams);

		if (m_DAFVoiceSpeakerOverlay)
			m_DAFVoiceSpeakerOverlay.RemoveSpeaker(vonStopParams.param2);
	}
}
