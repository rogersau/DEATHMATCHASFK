class KillFeedItem
{
	private Widget root;
	private Widget background;
	private TextWidget murderName;
	private TextWidget targetName;
	private TextWidget dst;
	private ImageWidget headshotIcon;
	private ItemPreviewWidget murderWeapon;

	private ref KillFeedWrapper parent;

	private EntityAI localWeapon;

	private bool destroyed;

	void KillFeedItem(KillFeedWrapper par, string mName, string tName, string wType, int dist, string msg, int type, int headshot = 0)
	{
		destroyed = false;
		parent = par;
		if (type)
		{
			root = GetGame().GetWorkspace().CreateWidgets("DAFDeathmatch/assets/murderinfo.layout", par.GetRoot());
			if (!root)
			{
				Print("DAFDeathmatch: failed to create killfeed item widget");
				destroyed = true;
				return;
			}

			murderName = TextWidget.Cast(root.FindAnyWidget("MurderName"));
			targetName = TextWidget.Cast(root.FindAnyWidget("TargetName"));
			dst = TextWidget.Cast(root.FindAnyWidget("KillDst"));
			headshotIcon = ImageWidget.Cast(root.FindAnyWidget("HeadshotIcon"));
			murderWeapon = ItemPreviewWidget.Cast(root.FindAnyWidget("MurderWeapon"));

			if (murderName)
				murderName.SetText(FormatNick(mName));
			if (targetName)
				targetName.SetText(FormatNick(tName));
			
			string distanceText = dist.ToString() + "m";
			if (dst)
				dst.SetText(distanceText);
			if (headshotIcon)
				headshotIcon.Show(headshot == 1);

			SetWeapon(wType);
		}
		else
		{
			root = GetGame().GetWorkspace().CreateWidgets("DAFDeathmatch/assets/murderinfoextra.layout", par.GetRoot());
			if (!root)
			{
				Print("DAFDeathmatch: failed to create killfeed death message widget");
				destroyed = true;
				return;
			}

			targetName = TextWidget.Cast(root.FindAnyWidget("TargetName"));
			murderName = TextWidget.Cast(root.FindAnyWidget("DeathMsg"));
			if (murderName)
				murderName.SetText(msg);
			if (targetName)
				targetName.SetText(FormatNick(tName));
		}

		background = root.FindAnyWidget("Background");

		float width, height;
		width = GetFeedWidth();
		if (background)
		{
			background.GetSize(null, height);
			background.SetSize(width, height);
		}


		GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.Destroy, 5000, false);
	}

	string FormatNick(string source)
	{
		string result = source;
		if (result.Length() > 24)
			result = result.Substring(0, 20) + "...";
		return result;
	}

	void SetWeapon(string data)
	{
		if (data == string.Empty)
			return;

		if (!murderWeapon)
			return;

		TStringArray parts = new TStringArray;
		data.Split(",", parts);

		if (parts.Count() == 0)
			return;

		string weaponType = GetWeaponPartType(parts.Get(0));
		EntityAI weapon = EntityAI.Cast(GetGame().CreateObject(weaponType, vector.Zero, true));
		if (!weapon)
			return;

		array<EntityAI> parents = new array<EntityAI>();
		parents.Insert(weapon);

		for (int i = 1; i < parts.Count(); i++)
		{
			string attachmentPart = parts.Get(i);
			int depth = GetWeaponPartDepth(attachmentPart);
			string attachmentType = GetWeaponPartType(attachmentPart);

			EntityAI parentItem = weapon;
			if (depth > 0 && parents.Count() > depth - 1 && parents.Get(depth - 1))
				parentItem = parents.Get(depth - 1);

			EntityAI attachment;
			if (IsWeaponPartMagazine(attachmentPart))
			{
				Weapon_Base parentWeapon = Weapon_Base.Cast(parentItem);
				if (parentWeapon)
					parentWeapon.SpawnAmmo(attachmentType, Weapon_Base.SAMF_DEFAULT);
			}
			else
			{
				attachment = parentItem.GetInventory().CreateAttachment(attachmentType);
			}

			if (attachment)
			{
				while (parents.Count() <= depth)
				{
					parents.Insert(null);
				}

				parents.Set(depth, attachment);
			}
		}

		murderWeapon.SetItem(weapon);
		murderWeapon.SetView(weapon.GetViewIndex());
		murderWeapon.SetModelOrientation("0 0 0");
		localWeapon = weapon;
	}

	int GetWeaponPartDepth(string data)
	{
		TStringArray partData = new TStringArray;
		data.Split(":", partData);

		if (partData.Count() < 2)
			return 0;

		return partData.Get(0).ToInt();
	}

	string GetWeaponPartType(string data)
	{
		TStringArray partData = new TStringArray;
		data.Split(":", partData);

		if (partData.Count() >= 3)
			return partData.Get(2);

		if (partData.Count() < 2)
			return data;

		return partData.Get(1);
	}

	bool IsWeaponPartMagazine(string data)
	{
		TStringArray partData = new TStringArray;
		data.Split(":", partData);

		return partData.Count() >= 3 && partData.Get(1) == "MAG";
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
		if (!murderName || !targetName)
			return 0;

		float start, end, width;
		murderName.GetPos(null, start);
		targetName.GetPos(null, end);
		targetName.GetSize(width, null);
		width = start + end + width;
		return width;
	}
}
