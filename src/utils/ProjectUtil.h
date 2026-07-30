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
	///
	/// The fallback directory names are matched ignoring case, because mod
	/// archives authored on Windows routinely deploy them as "calientetools/
	/// bodyslide".
	std::string GetProjectPath();

	/// Path to a fixed-name entry under the project path ("SliderSets",
	/// "SliderPresets", "RefTemplates.xml", ...), with the case corrected to
	/// whatever is actually on disk. Use this instead of appending the name to
	/// GetProjectPath(): on a case-sensitive filesystem an exact-case append
	/// silently enumerates nothing when the mod shipped a differently-cased
	/// directory.
	///
	/// Returns the requested path unchanged when no match exists, so callers
	/// still fail on the name they asked for. When building a path to a file
	/// that may not exist yet, resolve the directory with this and append the
	/// file name, so the new file keeps the caller's spelling.
	std::string GetProjectSubPath(const std::string& name);
} // namespace ProjectUtil
