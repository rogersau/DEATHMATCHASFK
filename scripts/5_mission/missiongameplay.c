modded class MissionGameplay
{
	private ref DAFVoiceSpeakerOverlay m_DAFVoiceSpeakerOverlay;

	override void OnEvent(EventType eventTypeId, Param params)
	{
		super.OnEvent(eventTypeId, params);

		if (!GetGame().IsClient())
			return;

		bool isVonEvent = eventTypeId == VONStartSpeakingEventTypeID || eventTypeId == VONStopSpeakingEventTypeID;
		if (!isVonEvent)
			return;

		if (!m_DAFVoiceSpeakerOverlay)
			m_DAFVoiceSpeakerOverlay = new DAFVoiceSpeakerOverlay();

		if (eventTypeId == VONStartSpeakingEventTypeID)
		{
			VONStartSpeakingEventParams startParams;
			if (Class.CastTo(startParams, params))
				m_DAFVoiceSpeakerOverlay.AddSpeaker(startParams.param2, startParams.param1);
		}
		else if (eventTypeId == VONStopSpeakingEventTypeID)
		{
			VONStopSpeakingEventParams stopParams;
			if (Class.CastTo(stopParams, params))
				m_DAFVoiceSpeakerOverlay.RemoveSpeaker(stopParams.param2);
		}
	}
}
