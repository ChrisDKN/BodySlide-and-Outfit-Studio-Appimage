/*
BodySlide and Outfit Studio
See the included LICENSE file
*/

#include "PlatformUtil.h"

#include <cerrno>

#ifndef _WINDOWS
	#include <algorithm>
	#include <cctype>
	#include <dirent.h>
	#include <map>
	#include <mutex>
	#include <sys/stat.h>
#endif

namespace {
#ifndef _WINDOWS
bool PathExists(const std::string& path) {
	struct stat st;
	return ::stat(path.c_str(), &st) == 0;
}

std::string ToLowerAscii(const std::string& s) {
	std::string result(s);
	std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return result;
}

// Directory listings, keyed by directory, mapping lowercased entry name to the
// real one. Populated lazily and never invalidated: every lookup tries the exact
// path first, so a file created after a directory was cached is still found by
// its real name. Only a case-mismatched lookup of a just-created file could go
// stale, which does not happen in practice.
std::map<std::string, std::map<std::string, std::string>> g_dirEntryCache;
std::mutex g_dirEntryCacheMutex;

// Real name of `wanted` within `dir` ignoring case, or empty if there is none.
std::string MatchEntryIgnoringCase(const std::string& dir, const std::string& wanted) {
	const std::string needle = ToLowerAscii(wanted);

	std::lock_guard<std::mutex> lock(g_dirEntryCacheMutex);

	auto cached = g_dirEntryCache.find(dir);
	if (cached == g_dirEntryCache.end()) {
		std::map<std::string, std::string> entries;

		if (DIR* handle = ::opendir(dir.c_str())) {
			while (struct dirent* entry = ::readdir(handle)) {
				const std::string name(entry->d_name);
				if (name == "." || name == "..")
					continue;

				// First spelling wins, so the result stays stable if a directory
				// really does hold names differing only by case.
				entries.emplace(ToLowerAscii(name), name);
			}
			::closedir(handle);
		}

		cached = g_dirEntryCache.emplace(dir, std::move(entries)).first;
	}

	auto match = cached->second.find(needle);
	return match == cached->second.end() ? std::string() : match->second;
}
#endif

// True for any fopen() mode that can create or modify the file.
bool IsWriteMode(const char* mode) {
	for (const char* m = mode; m && *m; ++m)
		if (*m == 'w' || *m == 'a' || *m == '+')
			return true;

	return false;
}

std::string backslash_to_slash(const std::string& s) {
	std::string sc(s);
	size_t len = sc.length();
	for (size_t i = 0; i < len; ++i)
		if (sc[i] == '\\')
			sc[i] = '/';
	return sc;
}
} // namespace

namespace PlatformUtil {
#ifdef _WINDOWS
// ACP wide to multibyte
std::string WideToMultiByteACP(const std::wstring& wstr) {
	if (wstr.empty())
		return std::string();

	int size_needed = WideCharToMultiByte(CP_ACP, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_ACP, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, nullptr, nullptr);
	return strTo;
}

// UTF-8 multibyte to wide
std::wstring MultiByteToWideUTF8(const std::string& str) {
	if (str.empty())
		return std::wstring();

	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), nullptr, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}
#endif

std::string ResolveCaseInsensitivePath(const std::string& path) {
#ifdef _WINDOWS
	return path;
#else
	if (path.empty() || PathExists(path))
		return path;

	const bool absolute = (path[0] == '/');
	std::string resolved = absolute ? "/" : "";

	for (size_t i = absolute ? 1 : 0; i < path.size();) {
		const size_t separator = path.find('/', i);
		const size_t end = (separator == std::string::npos) ? path.size() : separator;
		const std::string part = path.substr(i, end - i);
		i = (separator == std::string::npos) ? path.size() : separator + 1;

		if (part.empty())
			continue;

		const auto join = [&resolved](const std::string& name) {
			if (resolved.empty())
				return name;
			if (resolved == "/")
				return "/" + name;
			return resolved + "/" + name;
		};

		// Prefer the spelling that was asked for; only fall back to a scan when
		// that component genuinely is not there. This keeps a fully correct path
		// free of directory listings even when an earlier component was fixed.
		const std::string candidate = join(part);
		if (PathExists(candidate)) {
			resolved = candidate;
			continue;
		}

		const std::string match = MatchEntryIgnoringCase(resolved.empty() ? "." : resolved, part);
		if (match.empty())
			return path; // Nothing matches; let the caller fail on what it asked for.

		resolved = join(match);
	}

	return resolved.empty() ? path : resolved;
#endif
}

void OpenFileStream(std::fstream& file, const std::string& fileName, std::ios_base::openmode mode) {
#ifdef _WINDOWS
	// Convert to std::wstring on Windows only
	file.open(MultiByteToWideUTF8(fileName).c_str(), mode);
#else
	std::string fn_nobs = backslash_to_slash(fileName);
	file.open(fn_nobs.c_str(), mode);

	// Retry a failed read with the case corrected. Deliberately not done for
	// writes: creating a file must use the requested name rather than silently
	// landing on an existing entry that merely resembles it.
	constexpr std::ios_base::openmode writeFlags = std::ios::out | std::ios::app | std::ios::trunc;
	const bool forWriting = (mode & writeFlags) != std::ios_base::openmode();

	if (!file && !forWriting) {
		const std::string resolved = ResolveCaseInsensitivePath(fn_nobs);
		if (resolved != fn_nobs) {
			file.clear();
			file.open(resolved.c_str(), mode);
		}
	}
#endif
}

bool FileExists(const std::string& fileName) {
	std::fstream file;
	PlatformUtil::OpenFileStream(file, fileName, std::ios::in | std::ios::binary);

	if (!file)
		return false;

	return true;
}

FILE* OpenFile(const std::string& fileName, const char* mode, int& error) {
	error = 0;

#ifdef _WINDOWS
	FILE* fp = nullptr;
	error = _wfopen_s(&fp, MultiByteToWideUTF8(fileName).c_str(), MultiByteToWideUTF8(mode).c_str());
	if (error || !fp) {
		if (!error)
			error = errno;

		return nullptr;
	}

	return fp;
#else
	const std::string name = backslash_to_slash(fileName);
	if (FILE* fp = fopen(name.c_str(), mode))
		return fp;

	// Same rule as OpenFileStream: only reads are retried with the case
	// corrected, so creating a file never lands on an existing entry that
	// merely resembles the requested name.
	if (!IsWriteMode(mode)) {
		const std::string resolved = ResolveCaseInsensitivePath(name);
		if (resolved != name) {
			if (FILE* fp = fopen(resolved.c_str(), mode))
				return fp;
		}
	}

	error = errno;
	return nullptr;
#endif
}

// Provide std::wstring function for Windows
#ifdef _WINDOWS
void OpenFileStream(std::fstream& file, const std::wstring& fileName, unsigned int mode) {
	file.open(fileName.c_str(), mode);
}

bool FileExists(const std::wstring& fileName) {
	std::fstream file;
	PlatformUtil::OpenFileStream(file, fileName, std::ios::in | std::ios::binary);

	if (!file)
		return false;

	return true;
}
#endif
} // namespace PlatformUtil
