class KillFeedWrapper
{
	private Widget root;
	private Widget playerCountRoot;
	private TextWidget playerCount;
	private ref array<ref KillFeedItem> items;

	void KillFeedWrapper()
	{
		root = GetGame().GetWorkspace().CreateWidgets("DAFImprovements/assets/FeedWrapper.layout", null);
		playerCountRoot = GetGame().GetWorkspace().CreateWidgets("DAFImprovements/assets/PlayerCount.layout", null);
		playerCount = TextWidget.Cast(playerCountRoot.FindAnyWidget("PlayerCount"));
		items = new array<ref KillFeedItem>();
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
		if (playerCount)
			playerCount.SetText("Players: " + count.ToString());
	}

	void Destroy()
	{
		for (int i = items.Count() - 1; i >= 0; i--)
		{
			KillFeedItem item = items.Get(i);
			if (item)
				item.Destroy();
		}

		if (playerCountRoot)
		{
			playerCountRoot.Unlink();
			playerCountRoot = null;
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
