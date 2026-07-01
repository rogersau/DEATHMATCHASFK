modded class SalineBagIV
{
	override bool CanPutInCargo(EntityAI parent)
	{
		if (!super.CanPutInCargo(parent))
			return false;

		return parent && parent.GetHierarchyRootPlayer();
	}
}
