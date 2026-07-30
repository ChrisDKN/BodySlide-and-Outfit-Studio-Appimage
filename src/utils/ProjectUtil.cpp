/*
BodySlide and Outfit Studio
See the included LICENSE file
*/

#include "ProjectUtil.h"
#include "ConfigurationManager.h"
#include "StringStuff.h"

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

extern ConfigurationManager Config;

std::string ProjectUtil::GetExeDir() {
	return std::string(wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath().ToUTF8());
}

std::string ProjectUtil::GetDataDir() {
	wxString envDir;
	if (wxGetEnv("BSOS_APPDIR", &envDir) && !envDir.IsEmpty()) {
		// Strip any trailing separator so callers can keep appending "/name".
		while (envDir.length() > 1 && (envDir.Last() == '/' || envDir.Last() == wxFileName::GetPathSeparator()))
			envDir.RemoveLast();

		return std::string(envDir.ToUTF8());
	}

	return GetExeDir();
}

std::string ProjectUtil::GetProjectPath() {
	std::string projectPath = Config["ProjectPath"];
	std::string appDir = Config["AppDir"];
	std::string gameDataPath = Config["GameDataPath"];

	// Build list of paths to check in order of preference
	std::vector<std::string> pathsToCheck;

	// First priority: configured ProjectPath (if set)
	if (!projectPath.empty()) {
		pathsToCheck.push_back(projectPath);
	}

	// Check if SliderSets subdirectory exists in appDir, but return appDir if it does
	if (wxDir::Exists(appDir + PathSepStr + "SliderSets")) {
		return appDir;
	}

	// Fallback paths in order of preference
	pathsToCheck.push_back(gameDataPath + PathSepStr + "CalienteTools" + PathSepStr + "BodySlide");
	pathsToCheck.push_back(gameDataPath + PathSepStr + "Tools" + PathSepStr + "BodySlide");

	// Return first existing path
	for (const auto& path : pathsToCheck) {
		if (wxDir::Exists(path)) {
			return path;
		}
	}

	// If no path exists, return projectPath if configured, otherwise AppDir
	return !projectPath.empty() ? projectPath : appDir;
}
