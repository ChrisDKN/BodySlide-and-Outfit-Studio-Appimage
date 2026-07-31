/*
BodySlide and Outfit Studio
See the included LICENSE file
*/

#pragma once

/// Filtering for GTK3 log messages that neither program can do anything about.
///
/// Every entry point is a no-op unless the build actually targets GTK3 (the
/// build system defines BSOS_HAVE_GTK3 when it finds it), so callers do not need
/// their own platform guards.
namespace GtkLogUtil {

#ifndef BSOS_HAVE_GTK3

inline void FilterKnownWarnings() {}

#else

/// Drop the Gtk-CRITICAL that scrolling a wxScrolledWindow with hidden children
/// produces, and pass everything else through untouched.
///
/// wxPizza::scroll() shifts the allocation of every child of the scrolled
/// window so fast scrolling cannot leave one painted in the old place, and it
/// walks the children with gtk_container_forall(), which does not skip the
/// hidden ones. gtk_widget_set_allocation() rejects a widget that is not
/// visible, so each hidden child logs
///
///   gtk_widget_set_allocation: assertion '_gtk_widget_get_visible (widget) ||
///   _gtk_widget_is_toplevel (widget)' failed
///
/// per scroll step. Both programs keep plenty of hidden children in their
/// slider panels -- the slider pool, the low-weight column when the set is
/// one-size, the checkbox/slider pair a zap does not use -- so a single flick of
/// the wheel emits dozens of these. The allocation of a hidden widget is
/// meaningless anyway; it gets a fresh one from the sizer when it is shown
/// again, so nothing is actually wrong.
///
/// Fixing it belongs in wxWidgets (the callback needs a
/// gtk_widget_get_visible() check), so until that lands the message is dropped
/// by exact text. Anything else GTK logs is still printed.
void FilterKnownWarnings();

#endif

} // namespace GtkLogUtil
