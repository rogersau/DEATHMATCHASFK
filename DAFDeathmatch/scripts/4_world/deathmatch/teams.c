/**
 * DAFDMTeams owns player-to-team assignment for team deathmatch.
 *
 * Extracted from the DAFDeathmatch god class. Previously team state
 * (m_TeamByPlayerId) and seven team methods lived in the manager beside
 * spawn safety, loadouts, and arena walls. This module concentrates the
 * assignment logic behind one interface keyed on the active game mode.
 *
 * The active mode (ffa/tdm) is NOT owned here — it is round state and stays
 * in the manager. Every call takes the mode as a parameter, so this module
 * has no knowledge of rounds, arenas, or the round lifecycle. That keeps the
 * seam clean: the manager (round owner) passes the mode; teams module decides
 * assignment.
 *
 * Assignment rules (CONTEXT.md "Spawn Safety And Team Modes"):
 *   - Preassigned players keep their configured team.
 *   - Unassigned players fill the smaller team (balanced random with tiebreak).
 *   - Assignment is idempotent: a player keeps their first-assigned team until
 *     the round resets or Shuffle is called.
 *
 * Inventory outfit enforcement (ApplyTeamOutfit) stays in the manager because
 * it is loadout-mutation logic that merely consumes team state; it will move
 * with the loadout-engine slice.
 */
class DAFDMTeams
{
	private ref DAFDMSettings m_Settings;
	private ref map<string, string> m_TeamByPlayerId = new map<string, string>();

	void DAFDMTeams(DAFDMSettings settings)
	{
		m_Settings = settings;
	}

	/** True when the given mode resolves to team deathmatch. */
	static bool IsTeamMode(string mode)
	{
		mode.ToLower();
		return mode == "tdm";
	}

	/**
	 * Assign (or return the existing) team for a player under the given mode.
	 * Returns "" outside team mode or for a null identity. Idempotent per
	 * player until Reset/Shuffle.
	 */
	string Assign(PlayerIdentity identity, string mode)
	{
		if (!identity || !IsTeamMode(mode))
			return "";

		string playerId = identity.GetId();
		string existingTeam;
		if (m_TeamByPlayerId.Find(playerId, existingTeam))
			return existingTeam;

		string team = FindPreassignedTeam(playerId);
		if (team == "")
			team = PickBalancedTeam();

		m_TeamByPlayerId.Set(playerId, team);
		PrintFormat("DAFDeathmatch: assigned team player=%1 team=%2", playerId, team);
		return team;
	}

	/** Resolve the team for a player, assigning one if none exists yet. */
	string GetTeam(PlayerIdentity identity, string mode)
	{
		if (!identity)
			return "";

		string team;
		if (m_TeamByPlayerId.Find(identity.GetId(), team))
			return team;

		return Assign(identity, mode);
	}

	/** Drop all assignments. Called at round start. */
	void Reset()
	{
		m_TeamByPlayerId.Clear();
	}

	/** Number of players currently assigned to a team. */
	int CountAssigned(string team)
	{
		int count = 0;
		for (int i = 0; i < m_TeamByPlayerId.Count(); i++)
		{
			if (m_TeamByPlayerId.GetElement(i) == team)
				count++;
		}

		return count;
	}

	/** True when team is one of the configured team names (case-insensitive). */
	bool IsValidTeam(string team)
	{
		if (!m_Settings || !m_Settings.teamNames)
			return false;

		foreach (string configuredTeam: m_Settings.teamNames)
		{
			configuredTeam.ToLower();
			if (configuredTeam == team)
				return true;
		}

		return false;
	}

	private string FindPreassignedTeam(string playerId)
	{
		if (playerId == "" || !m_Settings || !m_Settings.preassignedTeams)
			return "";

		foreach (DAFDMTeamAssignmentConfig assignment: m_Settings.preassignedTeams)
		{
			if (!assignment || assignment.playerId != playerId)
				continue;

			string team = assignment.team;
			team.ToLower();
			if (IsValidTeam(team))
				return team;
		}

		return "";
	}

	private string PickBalancedTeam()
	{
		if (!m_Settings || !m_Settings.teamNames || m_Settings.teamNames.Count() == 0)
			return "red";

		string bestTeam = m_Settings.teamNames.GetRandomElement();
		int bestCount = 1000000;
		foreach (string team: m_Settings.teamNames)
		{
			if (team == "")
				continue;

			team.ToLower();
			int count = CountAssigned(team);
			if (count < bestCount || (count == bestCount && Math.RandomInt(0, 2) == 0))
			{
				bestTeam = team;
				bestCount = count;
			}
		}

		return bestTeam;
	}
}
