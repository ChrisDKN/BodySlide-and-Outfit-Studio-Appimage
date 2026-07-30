/*
BodySlide and Outfit Studio
See the included LICENSE file
*/

#include "FileSearchUtil.h"

#include <wx/filename.h>

void FileSearchUtil::GetFilesByExtension(const wxString& dirName, wxArrayString& files, const wxString& extension, int flags) {
	wxArrayString found;
	wxDir::GetAllFiles(dirName, &found, wxEmptyString, flags);

	for (const wxString& path : found) {
		if (wxFileName(path).GetExt().IsSameAs(extension, false))
			files.Add(path);
	}
}
