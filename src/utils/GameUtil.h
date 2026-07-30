/*
BodySlide and Outfit Studio
See the included LICENSE file
*/

#pragma once

#include <string>
#include <vector>
#include <array>
#include <wx/string.h>

namespace GameUtil {
	/// Shared array mapping target game indices to game names
	extern const std::array<wxString, 10> TargetGames;

	/// Index of a game name within TargetGames, or -1 if it is not a known game.
	/// Comparison is case-insensitive and ignores surrounding whitespace.
	int FindTargetGame(const wxString& name);

	/// Apply the BSOS_TARGET_GAME / BSOS_GAME_DATA_PATH / BSOS_OUTPUT_DATA_PATH
	/// environment overrides to the configuration.
	///
	/// Intended for launchers and mod managers that own the install: whatever
	/// they pass wins over the stored configuration on every launch, so the user
	/// never has to pick the game or its data folder by hand. A value edited in
	/// the settings dialog is therefore reverted the next time the launcher
	/// starts the program, which is the point -- the launcher is authoritative.
	///
	/// BSOS_TARGET_GAME accepts either the game name as it appears in
	/// TargetGames ("Fallout4", "SkyrimSpecialEdition"; case-insensitive) or the
	/// raw index. An unrecognised value is logged and ignored rather than
	/// silently selecting the wrong game. The two path variables are trimmed and
	/// given a trailing separator, which several call sites depend on.
	///
	/// Must be called after the configuration is loaded and before anything
	/// reads TargetGame or GameDataPath.
	void ApplyEnvironmentOverrides();

	/// Get the game data path for the specified target game.
	/// Checks config "GameDataPaths/<gamename>" first, then falls back to Windows registry on Windows.
	wxString GetGameDataPath(int targ);

	/// Initialize archive loading (BSA/BA2 files) for the currently configured game.
	void InitArchives();

	/// Get list of archive files (BSA/BA2) for the currently configured game.
	void GetArchiveFiles(std::vector<std::string>& outList);
}
