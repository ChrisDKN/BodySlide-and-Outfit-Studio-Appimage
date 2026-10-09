/*
BodySlide and Outfit Studio
See the included LICENSE file
*/

#include "GtkChoiceUtil.h"

#ifdef BSOS_HAVE_GTK3

#include <wx/app.h>
#include <wx/arrstr.h>
#include <wx/dialog.h>
#include <wx/dc.h>
#include <wx/intl.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/srchctrl.h>
#include <wx/weakref.h>
#include <wx/window.h>
#include <wx/vlbox.h>

#include <gtk/gtk.h>

namespace {

class ChoiceList : public wxVListBox {
public:
	ChoiceList(wxWindow* parent, const wxArrayString& items, const std::vector<int>& rows)
		: wxVListBox(parent, wxID_ANY), items(items), rows(rows) {}

protected:
	void OnDrawItem(wxDC& dc, const wxRect& rect, size_t row) const override {
		dc.SetFont(GetFont());
		dc.SetTextForeground(IsSelected(row) ? wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT) : GetForegroundColour());
		dc.DrawText(items[rows[row]], rect.x + FromDIP(4), rect.y + FromDIP(2));
	}

	wxCoord OnMeasureItem(size_t) const override {
		return GetCharHeight() + FromDIP(4);
	}

private:
	const wxArrayString& items;
	const std::vector<int>& rows;
};

class SearchableChoiceDialog : public wxDialog {
public:
	SearchableChoiceDialog(wxWindow* parent, const wxString& title, const wxArrayString& items, int selection)
		: wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(500, 600), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, allItems(items) {
		lowerItems.Alloc(items.size());
		for (const auto& item : items)
			lowerItems.Add(item.Lower());
		itemForRow.reserve(items.size());

		auto* sizer = new wxBoxSizer(wxVERTICAL);

		search = new wxSearchCtrl(this, wxID_ANY);
		search->ShowSearchButton(true);
		search->ShowCancelButton(true);
		search->SetDescriptiveText(_("Type to filter..."));
		sizer->Add(search, 0, wxEXPAND | wxALL, 5);

		listBox = new ChoiceList(this, allItems, itemForRow);
		sizer->Add(listBox, 1, wxEXPAND | wxLEFT | wxRIGHT, 5);

		sizer->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 5);
		SetSizer(sizer);

		ApplyFilter(wxEmptyString);
		if (selection >= 0 && selection < static_cast<int>(allItems.GetCount()))
			listBox->SetSelection(selection);

		search->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { ApplyFilter(search->GetValue()); });
		listBox->Bind(wxEVT_LISTBOX_DCLICK, [this](wxCommandEvent&) { EndModal(wxID_OK); });

		search->SetFocus();
		CenterOnParent();
	}

	~SearchableChoiceDialog() {
		listBox->SetItemCount(0);
	}

	/// Index into the item list the dialog was constructed with, or wxNOT_FOUND.
	int GetSelectedItem() const {
		int row = listBox->GetSelection();
		if (row == wxNOT_FOUND || row >= static_cast<int>(itemForRow.size()))
			return wxNOT_FOUND;

		return itemForRow[row];
	}

private:
	void ApplyFilter(const wxString& filter) {
		const wxString needle = filter.Lower();
		const int selected = GetSelectedItem();
		int selectedRow = wxNOT_FOUND;

		listBox->SetItemCount(0);
		itemForRow.clear();

		for (size_t i = 0; i < allItems.GetCount(); i++) {
			if (needle.empty() || lowerItems[i].Contains(needle)) {
				if (static_cast<int>(i) == selected)
					selectedRow = static_cast<int>(itemForRow.size());
				itemForRow.push_back(static_cast<int>(i));
			}
		}

		listBox->SetItemCount(itemForRow.size());
		if (!itemForRow.empty())
			listBox->SetSelection(selectedRow == wxNOT_FOUND ? 0 : selectedRow);
	}

	wxSearchCtrl* search = nullptr;
	ChoiceList* listBox = nullptr;
	wxArrayString allItems;
	wxArrayString lowerItems;
	std::vector<int> itemForRow; // listbox row -> index in allItems
};

/// Guards against a second dialog while one is already up: the press that opens
/// the dialog is swallowed, but GTK can still deliver another before the
/// deferred handler runs.
bool popupOpen = false;

void ShowChoiceDialog(wxChoice* choice) {
	wxArrayString items;
	const unsigned count = choice->GetCount();
	items.Alloc(count);
	for (unsigned i = 0; i < count; i++)
		items.Add(choice->GetString(i));

	SearchableChoiceDialog dlg(wxGetTopLevelParent(choice), choice->GetName(), items, choice->GetSelection());
	if (dlg.ShowModal() != wxID_OK)
		return;

	const int selected = dlg.GetSelectedItem();
	if (selected == wxNOT_FOUND)
		return;

	choice->SetSelection(selected);

	// wxChoice only emits this for user interaction with the native popup, which
	// we just suppressed, so it has to be raised by hand.
	wxCommandEvent event(wxEVT_CHOICE, choice->GetId());
	event.SetEventObject(choice);
	event.SetInt(selected);
	event.SetString(choice->GetString(selected));
	choice->ProcessWindowEvent(event);
}

void QueueChoiceDialog(wxChoice* control) {
	if (!popupOpen) {
		popupOpen = true;

		// The dialog runs its own event loop, which must not be entered from
		// inside a GTK signal emission, so defer it to the main loop. The choice
		// may be destroyed before that happens (settings changes rebuild the
		// frame), hence the weak reference.
		wxWeakRef<wxChoice> choice(control);
		wxTheApp->CallAfter([choice]() {
			if (choice)
				ShowChoiceDialog(choice.get());

			popupOpen = false;
		});
	}
}

gboolean OnComboEvent(GtkWidget*, GdkEvent* event, gpointer userData) {
	if (event->type != GDK_BUTTON_PRESS || event->button.button != GDK_BUTTON_PRIMARY)
		return FALSE;

	QueueChoiceDialog(static_cast<wxChoice*>(userData));
	return TRUE;
}

void OnComboPopup(GtkComboBox* combo, gpointer userData) {
	g_signal_stop_emission_by_name(combo, "popup");
	QueueChoiceDialog(static_cast<wxChoice*>(userData));
}

void FindToggleButton(GtkWidget* child, gpointer data) {
	auto** found = static_cast<GtkWidget**>(data);
	if (*found)
		return;

	if (GTK_IS_TOGGLE_BUTTON(child)) {
		*found = child;
		return;
	}

	if (GTK_IS_CONTAINER(child))
		gtk_container_forall(GTK_CONTAINER(child), FindToggleButton, data);
}

} // namespace

namespace GtkChoiceUtil {

bool BulkSetItems(wxChoice* choice, const std::vector<wxString>& items) {
	if (!choice)
		return false;

	auto* combo = static_cast<GtkWidget*>(choice->GetHandle());
	if (!combo || !GTK_IS_COMBO_BOX(combo))
		return false;

	GtkTreeModel* model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));
	if (!model || !GTK_IS_LIST_STORE(model))
		return false;

	// wxChoice puts the label in the first string column and client data, when
	// there is any, in a separate pointer column.
	int textColumn = -1;
	const int columnCount = gtk_tree_model_get_n_columns(model);
	for (int col = 0; col < columnCount; col++) {
		if (gtk_tree_model_get_column_type(model, col) == G_TYPE_STRING) {
			textColumn = col;
			break;
		}
	}

	if (textColumn < 0)
		return false;

	auto* store = GTK_LIST_STORE(model);

	// Hold a reference of our own: detaching drops the combo's.
	g_object_ref(model);
	gtk_combo_box_set_model(GTK_COMBO_BOX(combo), nullptr);

	gtk_list_store_clear(store);

	GtkTreeIter iter;
	for (const auto& item : items) {
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, textColumn, item.utf8_str().data(), -1);
	}

	gtk_combo_box_set_model(GTK_COMBO_BOX(combo), model);
	g_object_unref(model);

	choice->InvalidateBestSize();
	return true;
}

void BindSearchablePopup(wxChoice* choice) {
	if (!choice)
		return;

	auto* combo = static_cast<GtkWidget*>(choice->GetHandle());
	if (!combo || !GTK_IS_COMBO_BOX(combo))
		return;

	// "event" runs before GTK's existing button-press-event popup handler.
	GtkWidget* toggleButton = nullptr;
	gtk_container_forall(GTK_CONTAINER(combo), FindToggleButton, &toggleButton);
	if (toggleButton)
		g_signal_connect(toggleButton, "event", G_CALLBACK(OnComboEvent), choice);

	g_signal_connect(combo, "event", G_CALLBACK(OnComboEvent), choice);
	g_signal_connect(combo, "popup", G_CALLBACK(OnComboPopup), choice);
}

} // namespace GtkChoiceUtil

#endif // BSOS_HAVE_GTK3
