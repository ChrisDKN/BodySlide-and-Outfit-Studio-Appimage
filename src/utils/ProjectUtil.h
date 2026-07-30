/*
BodySlide and Outfit Studio
See the included LICENSE file
*/

#pragma once

#include <string>

namespace ProjectUtil {
	/// Directory the running executable lives in.
	/// Use this for locating sibling executables, never for writable data:
	/// in an AppImage this resolves inside the read-only mount.
	std::string GetExeDir();

	/// Directory holding the writable app data (Config.xml, SliderSets, logs, ...).
	/// Honours the BSOS_APPDIR environment variable when it is set to a non-empty
	/// path, otherwise falls back to GetExeDir(). The override lets a portable
	/// bundle (AppImage) or an external mod manager point the two programs at a
	/// writable, per-instance data directory while the binaries stay read-only.
	std::string GetDataDir();

	/// Get the project path with directory existence checks.
	/// If ProjectPath config is set and exists, returns it.
	/// Otherwise, checks fallback paths in order:
	///   1. AppDir/SliderSets
	///   2. GameDataPath/CalienteTools/BodySlide
	///   3. GameDataPath/Tools/BodySlide
	/// Falls back to AppDir if no configured path and no fallback exists.
	std::string GetProjectPath();
} // namespace ProjectUtil
