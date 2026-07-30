/*
BodySlide and Outfit Studio
See the included LICENSE file
*/

#include "wxStateButton.h"

wxIMPLEMENT_DYNAMIC_CLASS(wxStateButton, wxButton);

wxBEGIN_EVENT_TABLE(wxStateButton, wxButton)
	EVT_LEFT_DOWN(wxStateButton::mouseDown)
	EVT_LEFT_UP(wxStateButton::mouseReleased)
	EVT_PAINT(wxStateButton::paintEvent)
wxEND_EVENT_TABLE()

wxStateButton::wxStateButton()
	: wxButton() {}

wxStateButton::wxStateButton(wxWindow* parent,
							 wxWindowID id,
							 const wxString& label,
							 const wxPoint& pos,
							 const wxSize& size,
							 long style,
							 const wxValidator& validator,
							 const wxString& name,
							 const bool noState)
	: wxButton(parent, id, label, pos, size, style, validator, name) {
	m_bNoState = noState;
}

/*
 * Called by the system or by wxWidgets when the button needs to be redrawn.
 * Trigger it by calling Refresh(), which is what the state setters below do.
 */
void wxStateButton::paintEvent(wxPaintEvent& WXUNUSED(evt)) {
	wxPaintDC dc(this);
	render(dc);
}

void wxStateButton::render(wxDC& dc) {
	int w;
	int h;
	dc.GetSize(&w, &h);
	if (m_bChecked) {
		dc.SetBrush(*wxGREY_BRUSH);
		dc.SetTextForeground(wxColor(255, 255, 255));
	}
	else {
		dc.SetBrush(wxBrush(wxColor(64, 64, 64)));
		dc.SetTextForeground(*wxLIGHT_GREY);
	}
	dc.SetPen(*wxGREY_PEN);

	wxRect r = GetClientRect();
	dc.DrawRectangle(0, 0, w, h);
	dc.SetFont(wxSystemSettings::GetFont(wxSystemFont::wxSYS_DEFAULT_GUI_FONT));

	if (m_bPendingChanges)
		dc.DrawLabel("* " + GetLabel(), r, wxALIGN_CENTER_VERTICAL | wxALIGN_CENTER_HORIZONTAL);
	else
		dc.DrawLabel(GetLabel(), r, wxALIGN_CENTER_VERTICAL | wxALIGN_CENTER_HORIZONTAL);
}

void wxStateButton::SetPendingChanges(bool newPending) {
	if (m_bPendingChanges != newPending) {
		m_bPendingChanges = newPending;
		Refresh();
	}
}

void wxStateButton::mouseDown(wxMouseEvent& WXUNUSED(event)) {
	wxCommandEvent evt(wxEVT_COMMAND_BUTTON_CLICKED, GetId());
	evt.SetEventObject(this);
	ProcessEvent(evt);

	if (!evt.GetSkipped()) {
		m_bChecked = true;
		Refresh();
	}
}

void wxStateButton::mouseReleased(wxMouseEvent& event) {
	if (m_bNoState) {
		m_bChecked = false;
		Refresh();
	}
	event.Skip();
}
