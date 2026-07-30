/*
BodySlide and Outfit Studio
See the included LICENSE file
*/

#pragma once

#include <vector>

#include <wx/choice.h>
#include <wx/string.h>

/// Workarounds for wxChoice's GTK3 backend, which does not cope with the item
/// counts BodySlide reaches (thousands of outfits and presets).
///
/// Every entry point is a no-op unless the build actually targets GTK3 (the
/// build system defines BSOS_HAVE_GTK3 when it finds it), so callers do not need
/// their own platform guards.
namespace GtkChoiceUtil {

#ifndef BSOS_HAVE_GTK3

inline bool BulkSetItems(wxChoice*, const std::vector<wxString>&) {
	return false;
}
inline void BindSearchablePopup(wxChoice*) {}

#else

/// Replace every item of `choice` in one shot.
///
/// GtkComboBox recomputes its popup geometry on each row-inserted/row-deleted
/// signal, so a Clear() + Append() loop over a few thousand outfits freezes the
/// UI for seconds. Detaching the model for the duration suppresses those
/// signals and leaves a single recalculation at the end.
///
/// Returns false when the fast path does not apply (the widget is not backed by
/// a GtkComboBox over a GtkListStore); the caller must then fall back to the
/// portable Clear()/Append() path.
///
/// IMPORTANT: this must be the last thing done to the choice's items. wxChoice
/// reports GetCount() straight from the GTK model but keeps its client data in a
/// separate array that only its own Append()/Insert()/Delete() grow, so filling
/// the store here leaves that array empty. A following Append() then inserts
/// into it at index GetCount(), which is way past its end -- an out-of-bounds
/// heap write, seen as "malloc(): unaligned tcache chunk detected" or a segfault
/// somewhere later. Pass the complete list of items in one call, and use only
/// SetSelection()/GetString()/Clear() afterwards; Clear() puts both sides back
/// in sync. Assumes the choice carries no client data, which is true of every
/// choice this is used on.
bool BulkSetItems(wxChoice* choice, const std::vector<wxString>& items);

/// Replace the native drop-down with a searchable modal dialog.
///
/// A GtkComboBox popup tall enough to exceed 65535px overflows the Wayland
/// surface size limit and takes the process down with it, which BodySlide hits
/// with a normal-sized outfit list. The popup height cannot be constrained from
/// outside the widget: GtkComboBox's popup window is private, and its internal
/// list_position() overrides whatever scroll policy we set. So the popup is
/// suppressed entirely and a filterable list is shown instead -- which is the
/// better interaction for thousands of items regardless of the crash.
///
/// Safe to call on any platform and with a null argument; does nothing if the
/// widget's internals are not laid out as expected, leaving native behaviour.
void BindSearchablePopup(wxChoice* choice);

#endif

} // namespace GtkChoiceUtil
