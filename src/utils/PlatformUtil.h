/*
BodySlide and Outfit Studio
See the included LICENSE file
*/

#pragma once

#ifdef _WINDOWS
#include <Windows.h>
#endif

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>

namespace PlatformUtil {
#ifdef _WINDOWS
// ACP wide to multibyte
std::string WideToMultiByteACP(const std::wstring& wstr);

// UTF-8 multibyte to wide
std::wstring MultiByteToWideUTF8(const std::string& str);
#endif

/// Resolve a path against a case-sensitive filesystem, correcting the case of
/// each component when an exact match does not exist.
///
/// Mod content is authored on Windows, where paths are case-insensitive, so an
/// .osp or .nif routinely refers to "Meshes\Actors\Character" while the files on
/// disk are "meshes/actors/character". That costs nothing on Windows and breaks
/// on Linux. Archive lookups are unaffected (FSBSA lowercases both sides); this
/// is only about loose files.
///
/// Returns the corrected path, or the input unchanged when it already exists or
/// no case-insensitive match is found. On Windows this is the identity function.
std::string ResolveCaseInsensitivePath(const std::string& path);

/// Opens a file stream. On case-sensitive filesystems a failed *read* is retried
/// once via ResolveCaseInsensitivePath(). Writes are never redirected: creating a
/// file must use the name the caller chose.
void OpenFileStream(std::fstream& file, const std::string& fileName, std::ios_base::openmode mode);
bool FileExists(const std::string& fileName);

// Provide std::wstring function for Windows
#ifdef _WINDOWS
void OpenFileStream(std::fstream& file, const std::wstring& fileName, unsigned int mode);
bool FileExists(const std::wstring& fileName);
#endif
} // namespace PlatformUtil

// This user-defined literal allows you to specify an int with a
// multi-character string: "OSD\0"_mci
inline constexpr uint32_t operator"" _mci(const char* p, size_t n) {
	uint32_t v = 0;
	for (size_t i = 0; i < n; ++i) {
		v <<= 8;
		v += static_cast<unsigned char>(p[i]);
	}
	return v;
}
