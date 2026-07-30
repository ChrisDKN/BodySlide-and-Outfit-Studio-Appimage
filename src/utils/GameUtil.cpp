/*
BodySlide and Outfit Studio
See the included LICENSE file
*/

#include "GameUtil.h"
#include "ConfigurationManager.h"
#include "FileSearchUtil.h"
#include "PlatformUtil.h"
#include "StringStuff.h"
#include "../../lib/FSEngine/FSManager.h"

#include <wx/dir.h>
#include <wx/log.h>
#include <wx/textfile.h>
#include <wx/tokenzr.h>
#include <wx/utils.h>

#include <algorithm>

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

#ifndef _WINDOWS
// Steam application IDs per target game, indexed to match TargetGames above.
// Several titles ship under more than one ID (base game vs. a re-release), so
// each entry lists every ID worth probing, most likely first.
const std::array<std::vector<wxUint32>, 10> SteamAppIds = {{
	{22370, 22300},	 // Fallout3 (Game of the Year, base)
	{22380, 22490},	 // FalloutNewVegas (base, regional)
	{72850},		 // Skyrim
	{377160},		 // Fallout4
	{489830},		 // SkyrimSpecialEdition
	{611660},		 // Fallout4VR
	{611670},		 // SkyrimVR
	{1151340},		 // Fallout76
	{22330, 900883}, // Oblivion (base, Game of the Year Deluxe)
	{1716740},		 // Starfield
}};

// Next `"..."` token on a line, starting at `pos` and advancing past it.
bool NextQuotedToken(const wxString& line, size_t& pos, wxString& outToken) {
	const size_t start = line.find('"', pos);
	if (start == wxString::npos)
		return false;

	const size_t end = line.find('"', start + 1);
	if (end == wxString::npos)
		return false;

	outToken = line.substr(start + 1, end - start - 1);
	pos = end + 1;
	return true;
}

// Values of `"<key>"  "<value>"` pairs in Valve's KeyValues text format.
// libraryfolders.vdf and appmanifest_*.acf only need flat lookups of keys that
// sit on one line, so this deliberately is not a full VDF parser.
void ReadVdfValues(const wxString& filePath, const wxString& key, std::vector<wxString>& outValues, bool firstOnly) {
	// Probing for a game that is not installed means opening manifests that are
	// not there, which is expected rather than an error. Without this every miss
	// would raise a wxLogError, and those surface as a modal dialog.
	wxLogNull suppressOpenErrors;

	wxTextFile file;
	if (!file.Open(filePath))
		return;

	for (size_t i = 0; i < file.GetLineCount(); i++) {
		size_t pos = 0;
		wxString foundKey;
		wxString value;

		if (!NextQuotedToken(file[i], pos, foundKey) || foundKey != key)
			continue;

		if (!NextQuotedToken(file[i], pos, value))
			continue;

		outValues.push_back(value);
		if (firstOnly)
			break;
	}

	file.Close();
}

std::vector<wxString> FindSteamLibraries() {
	std::vector<wxString> libraries;

	wxString home;
	if (!wxGetEnv("HOME", &home) || home.IsEmpty())
		return libraries;

	const auto addUnique = [&libraries](const wxString& path) {
		if (path.IsEmpty() || !wxDir::Exists(path))
			return;

		if (std::find(libraries.begin(), libraries.end(), path) == libraries.end())
			libraries.push_back(path);
	};

	// ~/.steam/steam and ~/.steam/root are symlinks Steam maintains; the other
	// two are the native and Flatpak data directories.
	const char* const relativeRoots[] = {
		"/.steam/steam",
		"/.steam/root",
		"/.local/share/Steam",
		"/.var/app/com.valvesoftware.Steam/data/Steam",
	};

	for (const char* relative : relativeRoots) {
		const wxString root = home + relative;
		if (!wxDir::Exists(root))
			continue;

		addUnique(root);

		// Libraries on other drives -- a second SSD, or the Steam Deck's SD card
		// under /run/media -- are listed here rather than under the home root.
		std::vector<wxString> paths;
		ReadVdfValues(root + "/steamapps/libraryfolders.vdf", "path", paths, false);
		for (const wxString& path : paths)
			addUnique(path);
	}

	return libraries;
}

const std::vector<wxString>& GetSteamLibraries() {
	// Scanned once: the set of libraries does not change while we run, and
	// GetGameDataPath() is called repeatedly while the settings dialog is open.
	static const std::vector<wxString> libraries = FindSteamLibraries();
	return libraries;
}

// Data directory of a Steam-installed game, or empty when it is not installed.
// The install directory name is read from the app manifest rather than guessed,
// so regional and re-released editions resolve to the right folder.
wxString FindSteamGameDataPath(int targ) {
	if (targ < 0 || targ >= static_cast<int>(SteamAppIds.size()))
		return wxEmptyString;

	for (const wxString& library : GetSteamLibraries()) {
		for (wxUint32 appId : SteamAppIds[targ]) {
			const wxString manifest = wxString::Format("%s/steamapps/appmanifest_%u.acf", library, appId);

			std::vector<wxString> installDirs;
			ReadVdfValues(manifest, "installdir", installDirs, true);
			if (installDirs.empty())
				continue;

			const std::string dataDir = std::string((library + "/steamapps/common/" + installDirs[0] + "/Data").ToUTF8());
			const wxString resolved = wxString::FromUTF8(PlatformUtil::ResolveCaseInsensitivePath(dataDir));
			if (wxDir::Exists(resolved))
				return resolved + PathSepChar;
		}
	}

	return wxEmptyString;
}
#endif
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
#else
	else {
		// Linux counterpart to the registry lookup above. Without it an
		// unconfigured game has nowhere to come from, so the first-run setup
		// dialog opens with every path blank and its buttons disabled.
		dataPath = FindSteamGameDataPath(targ);
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
	FileSearchUtil::GetFilesByExtension(dataDir, files, "ba2", wxDIR_FILES);
	FileSearchUtil::GetFilesByExtension(dataDir, files, "bsa", wxDIR_FILES);
	for (auto& f : files) {
		f = f.AfterLast('/').AfterLast('\\');
		if (fsearch.find(f.Lower()) == fsearch.end())
			outList.push_back((dataDir + f).ToUTF8().data());
	}
}
