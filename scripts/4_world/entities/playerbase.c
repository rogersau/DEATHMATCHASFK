modded class PlayerBase extends ManBase
{
	private ref KillFeedWrapper killFeed;
	private ref KillFeedHandle killHandle;
	private bool IsFallDeath = false;
	private bool IsHeadshot = false;

	void PlayerBase()
	{
		if (GetGame().IsClient())
		{
			killFeed = new KillFeedWrapper();
			killHandle = new KillFeedHandle();
		}		
	}
	void Init()
	{
		super.Init();
	}
	override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
	{
		super.OnRPC(sender, rpc_type, ctx);
		
		if (GetGame().IsClient())
		{
			if (rpc_type == -74700005)
			{
				Param6<string, string, string, int, string, int> data;
				if (!ctx.Read(data))
					return;
				killFeed.AddItem(data);
			}
		}
	}
}