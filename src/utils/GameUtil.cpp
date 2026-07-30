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
struct SkeletonDefault {
	const char* reference;
	const char* rootName;
};

// Indexed to match TargetGames above (and the TargetGame enum):
// FO3, FONV, SKYRIM, FO4, SKYRIMSE, FO4VR, SKYRIMVR, FO76, OB, SF
//
// Mirrors the assignments the first-run setup dialog makes in
// BodySlideApp::ShowSetup() / OutfitStudio::ShowSetup(). Fallout 76 is blank
// because no skeleton for it ships in res/, matching that dialog's switch.
const std::array<SkeletonDefault, 10> SkeletonDefaults = {{
	{"res/skeleton_fo3nv.nif", "Bip01"},				// Fallout3
	{"res/skeleton_fo3nv.nif", "Bip01"},				// FalloutNewVegas
	{"res/skeleton_female_sk.nif", "NPC Root [Root]"},	// Skyrim
	{"res/skeleton_fo4.nif", "Root"},					// Fallout4
	{"res/skeleton_female_sse.nif", "NPC Root [Root]"}, // SkyrimSpecialEdition
	{"res/skeleton_fo4.nif", "Root"},					// Fallout4VR
	{"res/skeleton_female_sse.nif", "NPC Root [Root]"}, // SkyrimVR
	{"", ""},											// Fallout76 (none shipped)
	{"res/skeleton_ob.nif", "Bip01"},					// Oblivion
	{"res/skeleton_female_sf.nif", "Root"},				// Starfield
}};

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

bool GameUtil::GetDefaultSkeleton(int targ, std::string& outReference, std::string& outRootName) {
	if (targ < 0 || targ >= static_cast<int>(SkeletonDefaults.size()))
		return false;

	const SkeletonDefault& def = SkeletonDefaults[targ];
	if (def.reference[0] == '\0')
		return false;

	outReference = def.reference;
	outRootName = def.rootName;
	return true;
}

void GameUtil::ApplyEnvironmentOverrides() {
	// Read before anything is overwritten, so we can tell an actual game change
	// from a launch that merely restates the game already configured.
	const int previousGame = Config.GetIntValue("TargetGame", -1);

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

			// The skeleton reference is normally written by the first-run setup
			// dialog, which only runs when TargetGame is unset -- and setting the
			// game here means it never will. Left empty, OutfitProject falls back
			// to AppDir + "" and reports "Failed to load skeleton '<data dir>/'".
			//
			// Only fill it in when the game actually changed or nothing is set, so
			// that a skeleton the user picked by hand for this same game survives.
			std::string skeletonRef;
			std::string skeletonRoot;
			if ((targetGame != previousGame || Config["Anim/DefaultSkeletonReference"].empty())
				&& GameUtil::GetDefaultSkeleton(targetGame, skeletonRef, skeletonRoot)) {
				Config.SetValue("Anim/DefaultSkeletonReference", skeletonRef);
				Config.SetValue("Anim/SkeletonRootName", skeletonRoot);
				wxLogMessage("Default skeleton set to %s (root '%s').", skeletonRef, skeletonRoot);
			}
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
