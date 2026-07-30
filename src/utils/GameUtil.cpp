/*
BodySlide and Outfit Studio
See the included LICENSE file
*/

#include "GameUtil.h"
#include "ConfigurationManager.h"
#include "StringStuff.h"
#include "../../lib/FSEngine/FSManager.h"

#include <wx/dir.h>
#include <wx/log.h>
#include <wx/tokenzr.h>
#include <wx/utils.h>

#ifdef _WINDOWS
	#include <wx/msw/registry.h>
#endif

extern ConfigurationManager Config;

// TargetGames mapping - must match enum in BodySlideApp.h and OutfitStudio.h
// enum TargetGame { FO3, FONV, SKYRIM, FO4, SKYRIMSE, FO4VR, SKYRIMVR, FO76, OB, SF };
namespace GameUtil {
const std::array<wxString, 10> TargetGames = {
	"Fallout3", "FalloutNewVegas", "Skyrim", "Fallout4", "SkyrimSpecialEdition",
	"Fallout4VR", "SkyrimVR", "Fallout76", "Oblivion", "Starfield"
};
} // namespace GameUtil

namespace {
// Reads a directory path from the environment, trimmed and with a trailing
// separator. Returns false when the variable is unset or empty.
bool GetEnvDirPath(const char* name, wxString& outPath) {
	wxString value;
	if (!wxGetEnv(name, &value))
		return false;

	value.Trim(true).Trim(false);
	if (value.IsEmpty())
		return false;

	// Callers concatenate these values with bare file names and relative paths
	// (see GetArchiveFiles() and GetOutputDataPath()), so the trailing separator
	// is load-bearing rather than cosmetic. The settings dialog stores them the
	// same way, via wxFileName::GetDirName().
	if (!value.EndsWith(PathSepStr))
		value.Append(PathSepChar);

	outPath = value;
	return true;
}
} // namespace

int GameUtil::FindTargetGame(const wxString& name) {
	wxString needle = name;
	needle.Trim(true).Trim(false);

	for (size_t i = 0; i < GameUtil::TargetGames.size(); i++)
		if (GameUtil::TargetGames[i].IsSameAs(needle, false))
			return static_cast<int>(i);

	return -1;
}

void GameUtil::ApplyEnvironmentOverrides() {
	int targetGame = -1;

	wxString envGame;
	if (wxGetEnv("BSOS_TARGET_GAME", &envGame) && !envGame.IsEmpty()) {
		targetGame = GameUtil::FindTargetGame(envGame);

		if (targetGame < 0) {
			// Also accept the raw index, so a caller can pass whichever form it
			// already has without having to map between the two.
			long index = 0;
			wxString trimmed = envGame;
			trimmed.Trim(true).Trim(false);
			if (trimmed.ToLong(&index) && index >= 0 && index < static_cast<long>(GameUtil::TargetGames.size()))
				targetGame = static_cast<int>(index);
		}

		if (targetGame >= 0) {
			Config.SetValue("TargetGame", targetGame);
			wxLogMessage("BSOS_TARGET_GAME: selected %s.", GameUtil::TargetGames[targetGame]);
		}
		else {
			// Picking a wrong game silently would load the wrong skeleton and
			// the wrong archives, so leave the configured value alone instead.
			wxLogWarning("BSOS_TARGET_GAME is '%s', which is not a known game; ignoring it.", envGame);
		}
	}

	wxString gameDataPath;
	if (GetEnvDirPath("BSOS_GAME_DATA_PATH", gameDataPath)) {
		Config.SetValue("GameDataPath", gameDataPath.ToUTF8().data());

		// Mirror it into the per-game slot the settings dialog maintains, so
		// switching game in the UI and back does not lose the path.
		if (targetGame < 0)
			targetGame = Config.GetIntValue("TargetGame");

		if (targetGame >= 0 && targetGame < static_cast<int>(GameUtil::TargetGames.size()))
			Config.SetValue("GameDataPaths/" + GameUtil::TargetGames[targetGame].ToStdString(), gameDataPath.ToUTF8().data());

		wxLogMessage("BSOS_GAME_DATA_PATH: using %s.", gameDataPath);
	}

	// Where built meshes are written. Left unset this falls back to
	// GameDataPath (see BodySlideApp::GetOutputDataPath), which is the right
	// default for a plain install but not when a mod manager wants the output
	// captured into a mod folder rather than dropped into the game.
	wxString outputPath;
	if (GetEnvDirPath("BSOS_OUTPUT_DATA_PATH", outputPath)) {
		Config.SetValue("OutputDataPath", outputPath.ToUTF8().data());
		wxLogMessage("BSOS_OUTPUT_DATA_PATH: writing build output to %s.", outputPath);
	}
}

wxString GameUtil::GetGameDataPath(int targ) {
	wxString dataPath;
	wxString gamestr = GameUtil::TargetGames[targ];
	wxString cust = "GameDataPaths/" + gamestr;

	if (!Config[cust].IsEmpty()) {
		dataPath = Config[cust];
	}
#ifdef _WINDOWS
	else {
		wxString gkey = "GameRegKey/" + gamestr;
		wxString gval = "GameRegVal/" + gamestr;
		std::string gameKey = Config[gkey].ToStdString();
		// Try to read from registry if available
		#ifdef wxUSE_REGKEY
			wxRegKey key(wxRegKey::HKLM, gameKey, wxRegKey::WOW64ViewMode_32);
			if (!gameKey.empty() && key.Exists()) {
				if (key.HasValues() && key.QueryValue(Config[gval], dataPath)) {
					dataPath.Append("Data").Append(PathSepChar);
				}
			}
		#endif
	}
#endif
	return dataPath;
}

void GameUtil::InitArchives() {
	// Auto-detect archives
	FSManager::del();

	std::vector<std::string> fileList;
	GetArchiveFiles(fileList);

	FSManager::addArchives(fileList);
}

void GameUtil::GetArchiveFiles(std::vector<std::string>& outList) {
	int targ = Config.GetIntValue("TargetGame");
	std::string cp = "GameDataFiles/" + GameUtil::TargetGames[targ].ToStdString();
	wxString activatedFiles = Config[cp];

	wxStringTokenizer tokenizer(activatedFiles, ";");
	std::map<wxString, bool> fsearch;
	while (tokenizer.HasMoreTokens()) {
		wxString val = tokenizer.GetNextToken().Trim(false);
		val = val.Trim().MakeLower();
		fsearch[val] = true;
	}

	wxString dataDir = Config["GameDataPath"];
	wxArrayString files;
	wxDir::GetAllFiles(dataDir, &files, "*.ba2", wxDIR_FILES);
	wxDir::GetAllFiles(dataDir, &files, "*.bsa", wxDIR_FILES);
	for (auto& f : files) {
		f = f.AfterLast('/').AfterLast('\\');
		if (fsearch.find(f.Lower()) == fsearch.end())
			outList.push_back((dataDir + f).ToUTF8().data());
	}
}
