class KillFeedItem
{
	private Widget root;
	private Widget background;
	private TextWidget murderName;
	private TextWidget targetName;
	private TextWidget dst;
	private ItemPreviewWidget murderWeapon;

	private ref KillFeedWrapper parent;

	private EntityAI localWeapon;

	private bool destroyed;

	void KillFeedItem(KillFeedWrapper par, string mName, string tName, string wType, int dist, string msg, int type)
	{
		destroyed = false;
		parent = par;
		if (type)
		{
			root = GetGame().GetWorkspace().CreateWidgets("PeppersKillfeed/assets/MurderInfo.layout", par.GetRoot());
			murderName = TextWidget.Cast(root.FindAnyWidget("MurderName"));
			targetName = TextWidget.Cast(root.FindAnyWidget("TargetName"));
			dst = TextWidget.Cast(root.FindAnyWidget("KillDst"));
			murderWeapon = ItemPreviewWidget.Cast(root.FindAnyWidget("MurderWeapon"));

			murderName.SetText(FormatNick(mName));
			targetName.SetText(FormatNick(tName));
			dst.SetText(String(dist.ToString() + "m."));

			SetWeapon(wType);
		}
		else
		{
			root = GetGame().GetWorkspace().CreateWidgets("PeppersKillfeed/assets/MurderInfoExtra.layout", par.GetRoot());
			targetName = TextWidget.Cast(root.FindAnyWidget("TargetName"));
			murderName = TextWidget.Cast(root.FindAnyWidget("DeathMsg"));
			murderName.SetText(msg);
			targetName.SetText(FormatNick(tName));
		}

		background = root.FindAnyWidget("Background");

		float width, height;
		width = GetFeedWidth();
		background.GetSize(null, height);
		background.SetSize(width, height);


		GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.Destroy, 5000, false);
	}

	string FormatNick(string source)
	{
		string result = source;
		if (result.Length() > 16)
			result = result.Substring(0, 12) + "...";
		return result;
	}

	void SetWeapon(string type)
	{
		EntityAI weapon = GetGame().CreateObject(type, vector.Zero, true);
		if (!weapon)
			return;
		murderWeapon.SetItem(weapon);
		murderWeapon.SetView(weapon.GetViewIndex());
		murderWeapon.SetModelOrientation("0 0 0");
		localWeapon = weapon;
	}

	void Destroy()
	{
		// Ensure Destroy is only executed once (idempotent). This lets both the 10s timer and wrapper-triggered destroy safely call Destroy().
		if (destroyed) return;
		destroyed = true;

		GetFeedWidth();
		parent.RemoveItem(this);
		if (root)
		{
			root.Unlink();
			root = null;
		}
		if (localWeapon)
		{
			GetGame().ObjectDelete(localWeapon);
			localWeapon = null;
		}
	}

	float GetFeedWidth()
	{
		float start, end, width;
		murderName.GetPos(null, start);
		targetName.GetPos(null, end);
		targetName.GetSize(width, null);
		width = start + end + width;

		Print("Width "+width);
		Print("start "+start);
		Print("end "+end);
		return width;
	}
}