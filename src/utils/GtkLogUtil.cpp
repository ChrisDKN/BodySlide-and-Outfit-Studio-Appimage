/*
BodySlide and Outfit Studio
See the included LICENSE file
*/

#include "GtkLogUtil.h"

#ifdef BSOS_HAVE_GTK3

#include <cstring>

#include <glib.h>

namespace {

/// The assertion wxPizza::scroll() trips on every hidden child, see the header.
/// Matched on both halves of the text so an unrelated failure of either the
/// same function or the same assertion still gets through.
bool IsHiddenChildAllocation(const gchar* message) {
	return message && std::strstr(message, "gtk_widget_set_allocation:") && std::strstr(message, "_gtk_widget_get_visible (widget)");
}

void LogHandler(const gchar* domain, GLogLevelFlags level, const gchar* message, gpointer) {
	if (IsHiddenChildAllocation(message))
		return;

	g_log_default_handler(domain, level, message, nullptr);
}

} // namespace

namespace GtkLogUtil {

void FilterKnownWarnings() {
	// The fatal and recursion flags travel in the level of the individual
	// message, and a handler is only picked for one whose flags it was
	// registered for; without them here the messages this is meant to catch
	// would bypass it whenever G_DEBUG marks them fatal.
	const auto levels = static_cast<GLogLevelFlags>(G_LOG_LEVEL_CRITICAL | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION);
	g_log_set_handler("Gtk", levels, LogHandler, nullptr);
}

} // namespace GtkLogUtil

#endif // BSOS_HAVE_GTK3
