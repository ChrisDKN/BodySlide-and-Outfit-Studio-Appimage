/*
BodySlide and Outfit Studio
See the included LICENSE file
*/

#pragma once

#include <wx/wx.h>

class wxStateButton : public wxButton {
	bool m_bChecked = false;
	bool m_bPendingChanges = false;
	bool m_bNoState = false;
	wxString text;

public:
	wxStateButton();
	wxStateButton(wxWindow* parent,
				  wxWindowID,
				  const wxString& label = wxEmptyString,
				  const wxPoint& pos = wxDefaultPosition,
				  const wxSize& size = wxDefaultSize,
				  long style = 0,
				  const wxValidator& validator = wxDefaultValidator,
				  const wxString& name = "button",
				  const bool noState = false);

	void paintEvent(wxPaintEvent& evt);

	/// Draws the button. Only ever called from the paint handler: drawing
	/// through a wxClientDC outside a paint event is unsupported on GTK3 and
	/// Wayland, where it is silently dropped or immediately overdrawn.
	void render(wxDC& dc);

	bool GetCheck() { return m_bChecked; }
	void SetCheck(bool newCheck = true) { m_bChecked = newCheck; }

	bool HasPendingChanges() { return m_bPendingChanges; }
	void SetPendingChanges(bool newPending = true);

	void mouseDown(wxMouseEvent& WXUNUSED(event));
	void mouseReleased(wxMouseEvent& event);

	DECLARE_DYNAMIC_CLASS(wxStateButton)
	DECLARE_EVENT_TABLE()
};
