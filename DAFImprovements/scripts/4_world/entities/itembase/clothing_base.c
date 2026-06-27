modded class Shoes_Base
{
	override bool CanDetachAttachment(EntityAI parent)
	{
		PlayerBase player = PlayerBase.Cast(parent);
		if (player)
			return false;

		return super.CanDetachAttachment(parent);
	}
}
