/*
BodySlide and Outfit Studio
See the included LICENSE file
*/

#pragma once

#include <wx/arrstr.h>
#include <wx/dir.h>
#include <wx/string.h>

namespace FileSearchUtil {
/// Appends every file under `dirName` whose extension matches `extension`
/// (given without the leading dot) to `files`, ignoring case.
///
/// wxDir::GetAllFiles() matches its filespec case-sensitively on Unix, so
/// "*.osp" misses OUTFIT.OSP and "*.bsa" misses ARCHIVE.BSA. Mod content is
/// authored on Windows, where that distinction does not exist, so the case a
/// file arrives with is effectively arbitrary.
///
/// `flags` is passed through to wxDir::GetAllFiles(); pass wxDIR_FILES to stay
/// out of subdirectories. Results are appended, so calling this once per
/// extension keeps each extension's matches grouped in the order requested.
void GetFilesByExtension(const wxString& dirName, wxArrayString& files, const wxString& extension, int flags = wxDIR_DEFAULT);
} // namespace FileSearchUtil
