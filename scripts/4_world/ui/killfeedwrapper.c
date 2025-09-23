class KillFeedWrapper
{
	private Widget root;
	private ref array<ref KillFeedItem> items;

	void KillFeedWrapper()
	{
		root = GetGame().GetWorkspace().CreateWidgets("PeppersKillfeed/assets/FeedWrapper.layout", null);
		items = new array<ref KillFeedItem>();
	}

	Widget GetRoot()
	{
		return root;
	}

	void AddItem(Param6<string, string, string, int, string, int> data)
	{
		// Insert new item at the end
		items.Insert(new KillFeedItem(this, data.param1, data.param2, data.param3, data.param4, data.param5, data.param6));

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