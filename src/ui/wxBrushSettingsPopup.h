/*
BodySlide and Outfit Studio
See the included LICENSE file
*/

#pragma once

#include "../program/OutfitStudio.h"

#include <wx/popupwin.h>

class wxBrushSettingsPopupBase {
protected:
	OutfitStudioFrame* os = nullptr;

	wxPanel* panel = nullptr;
	wxBoxSizer* topSizer = nullptr;
	wxFlexGridSizer* flexGridSizer = nullptr;

	wxStaticText* lbBrushSize = nullptr;
	wxSlider* brushSize = nullptr;
	wxTextCtrl* brushSizeVal = nullptr;

	wxStaticText* lbBrushStrength = nullptr;
	wxSlider* brushStrength = nullptr;
	wxTextCtrl* brushStrengthVal = nullptr;

	wxStaticText* lbBrushFocus = nullptr;
	wxSlider* brushFocus = nullptr;
	wxTextCtrl* brushFocusVal = nullptr;

	wxStaticText* lbBrushSpacing = nullptr;
	wxSlider* brushSpacing = nullptr;
	wxTextCtrl* brushSpacingVal = nullptr;

	wxBoxSizer* bottomSizer = nullptr;
	wxButton* buttonReset = nullptr;
	wxStaticText* brushNameLabel = nullptr;

	void Setup(wxWindow* popupWin);

public:
	wxBrushSettingsPopupBase(OutfitStudioFrame* parent, wxWindow* popupWin);

	void SetBrushName(const wxString& brushName);
	void SetBrushSize(float value);
	void SetBrushStrength(float value);
	void SetBrushFocus(float value);
	void SetBrushSpacing(float value);
};

class wxBrushSettingsPopupTransient : public wxPopupTransientWindow, public wxBrushSettingsPopupBase {
private:
	bool stayOpen = false;

	// wxWindow::IsMouseInWindow() is MSW-only, so test the pointer against the
	// parent's screen rectangle instead. That ignores occlusion, which errs
	// towards keeping the popup open -- the direction the setting asks for.
	bool IsMouseOverParent() const {
		const wxWindow* parent = GetParent();
		return parent && parent->GetScreenRect().Contains(wxGetMousePosition());
	}

public:
	wxBrushSettingsPopupTransient(OutfitStudioFrame* parent, bool stayOpen);

	// Anchored to a toolbar button, this popup is meant to survive clicks
	// elsewhere in the window. The override used to be MSW-only, so on GTK the
	// stay-open setting did nothing and any outside click closed the popup.
	//
	// Defers to the base rather than calling Hide(): outside MSW, Dismiss() also
	// removes the popup's event handlers, and hiding without that would leave
	// them installed on a hidden window.
	void Dismiss() override {
		if (!stayOpen || !IsMouseOverParent())
			wxPopupTransientWindow::Dismiss();
	}

#ifdef _WINDOWS
	void MSWDismissUnfocusedPopup() override {
		if (stayOpen)
			Dismiss();
	}
#endif
};
