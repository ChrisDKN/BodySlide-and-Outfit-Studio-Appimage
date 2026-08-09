/*
BodySlide and Outfit Studio
See the included LICENSE file
*/

#pragma once

#include <wx/scrolwin.h>
#include <wx/window.h>

/// Helpers for controls that live inside a scrolled window.
namespace WxScrollUtil {

/// Stops `window` from reacting to the mouse wheel and scrolls the scrolled
/// window it sits in instead.
///
/// A slider in a list of sliders is a trap: the wheel is how the list is moved,
/// but every native slider grabs the wheel to change its own value, so passing
/// over one while scrolling silently edits it. The control that happens to be
/// under the pointer should not decide what the wheel does when the pointer is
/// only travelling through it.
///
/// The event has to be handled rather than ignored: the control's native
/// handling runs unless the wheel event is marked as processed, and once it is
/// marked the event does not reach the scrolled window either. So the scroll is
/// done here, by the same step a wheel notch moves any other scrolled window --
/// GetLinesPerAction() scroll units, which is what both wxWidgets' own generic
/// handling and its GTK backend fall back on.
///
/// Does nothing if the window is not inside a wxScrolledWindow, apart from
/// still swallowing the wheel event.
inline void RedirectMouseWheelToScrollParent(wxWindow* window) {
	if (!window)
		return;

	window->Bind(wxEVT_MOUSEWHEEL, [carried = 0, lastAxis = int(wxMOUSE_WHEEL_VERTICAL)](wxMouseEvent& event) mutable {
		wxWindow* source = wxDynamicCast(event.GetEventObject(), wxWindow);

		wxScrolledWindow* scrolled = nullptr;
		for (wxWindow* parent = source ? source->GetParent() : nullptr; parent && !scrolled; parent = parent->GetParent())
			scrolled = wxDynamicCast(parent, wxScrolledWindow);

		if (!scrolled)
			return;

		if (int(event.GetWheelAxis()) != lastAxis) {
			lastAxis = int(event.GetWheelAxis());
			carried = 0;
		}

		// High resolution wheels and touchpads send rotations smaller than a
		// notch, so what is left over is carried into the next event instead of
		// being rounded away.
		const int wheelDelta = event.GetWheelDelta() > 0 ? event.GetWheelDelta() : 120;
		carried += event.GetWheelRotation();

		const int notches = carried / wheelDelta;
		if (notches == 0)
			return;

		carried -= notches * wheelDelta;

		const int orientation = event.GetWheelAxis() == wxMOUSE_WHEEL_HORIZONTAL ? wxHORIZONTAL : wxVERTICAL;
		int step = event.GetLinesPerAction() > 0 ? event.GetLinesPerAction() : 3;
		if (event.IsPageScroll())
			step = scrolled->GetScrollPageSize(orientation);

		// Rotation away from the user is positive and moves the view up.
		const int units = -notches * step;

		const wxPoint viewStart = scrolled->GetViewStart();
		if (orientation == wxHORIZONTAL)
			scrolled->Scroll(viewStart.x + units, viewStart.y);
		else
			scrolled->Scroll(viewStart.x, viewStart.y + units);
	});
}

} // namespace WxScrollUtil
