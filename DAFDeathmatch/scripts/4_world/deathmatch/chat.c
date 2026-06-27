class DAFDMChat
{
	static void MessagePlayer(Man player, string message)
	{
		if (!player || !player.GetIdentity())
			return;

		GetGame().RPCSingleParam(player, ERPCs.RPC_USER_ACTION_MESSAGE, new Param1<string>(message), true, player.GetIdentity());
	}

	static void Announce(string message)
	{
		array<Man> players = new array<Man>();
		GetGame().GetPlayers(players);

		foreach (Man player: players)
		{
			MessagePlayer(player, message);
		}
	}
}
