#include "main_window.h"

#include <glib/gi18n.h>

#include "app.h"

struct _DatakiMainWindow
{
  GtkApplicationWindow parent_instance;
};

G_DEFINE_TYPE(DatakiMainWindow, dataki_main_window, GTK_TYPE_APPLICATION_WINDOW)

/* -------------------------------------------------------------------------- */
/* Close -> hide to tray (handled by the application)                         */
/* -------------------------------------------------------------------------- */

static gboolean
on_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  (void)event;
  (void)user_data;

  GtkApplication *app = gtk_window_get_application(GTK_WINDOW(widget));
  if (DATAKI_IS_APP(app))
    return dataki_app_handle_window_close(DATAKI_APP(app));

  return FALSE; /* allow normal destroy */
}

/* -------------------------------------------------------------------------- */
/* Header bar with the ☰ menu                                                 */
/* -------------------------------------------------------------------------- */

static GtkWidget *
build_headerbar(void)
{
  GtkWidget *header = gtk_header_bar_new();
  gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
  gtk_header_bar_set_title(GTK_HEADER_BAR(header), "dataKI");

  /* ☰ primary menu (left) */
  GMenu *menu = g_menu_new();
  g_menu_append(menu, _("About dataKI"), "app.about");
  g_menu_append(menu, _("Quit"), "app.quit");

  GtkWidget *menu_button = gtk_menu_button_new();
  gtk_button_set_image(GTK_BUTTON(menu_button),
                       gtk_image_new_from_icon_name("open-menu-symbolic",
                                                    GTK_ICON_SIZE_BUTTON));
  gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_button), G_MENU_MODEL(menu));
  g_object_unref(menu);
  gtk_header_bar_pack_start(GTK_HEADER_BAR(header), menu_button);

  /* Model / effort placeholders (populated from bootstrap in a later phase) */
  GtkWidget *model_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(model_combo), _("Model"));
  gtk_combo_box_set_active(GTK_COMBO_BOX(model_combo), 0);
  gtk_widget_set_sensitive(model_combo, FALSE);
  gtk_header_bar_pack_start(GTK_HEADER_BAR(header), model_combo);

  GtkWidget *effort_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(effort_combo), _("Effort"));
  gtk_combo_box_set_active(GTK_COMBO_BOX(effort_combo), 0);
  gtk_widget_set_sensitive(effort_combo, FALSE);
  gtk_header_bar_pack_start(GTK_HEADER_BAR(header), effort_combo);

  /* Settings placeholder (right) */
  GtkWidget *settings_button = gtk_button_new_from_icon_name(
      "emblem-system-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_tooltip_text(settings_button, _("Settings"));
  gtk_widget_set_sensitive(settings_button, FALSE);
  gtk_header_bar_pack_end(GTK_HEADER_BAR(header), settings_button);

  return header;
}

/* -------------------------------------------------------------------------- */
/* Sidebar / chat view / composer placeholders                               */
/* -------------------------------------------------------------------------- */

static GtkWidget *
build_sidebar(void)
{
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_style_context_add_class(gtk_widget_get_style_context(box), "sidebar");
  gtk_widget_set_size_request(box, 240, -1);
  g_object_set(box, "margin", 8, NULL);

  GtkWidget *new_chat = gtk_button_new_with_label(_("New Chat"));
  gtk_widget_set_sensitive(new_chat, FALSE);
  gtk_box_pack_start(GTK_BOX(box), new_chat, FALSE, FALSE, 0);

  GtkWidget *search = gtk_search_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(search), _("Search chats…"));
  gtk_widget_set_sensitive(search, FALSE);
  gtk_box_pack_start(GTK_BOX(box), search, FALSE, FALSE, 0);

  GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  GtkWidget *list = gtk_list_box_new();
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);
  gtk_container_add(GTK_CONTAINER(scrolled), list);
  gtk_box_pack_start(GTK_BOX(box), scrolled, TRUE, TRUE, 0);

  return box;
}

static GtkWidget *
build_chat_area(void)
{
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  /* Chat view placeholder */
  GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_set_vexpand(scrolled, TRUE);

  GtkWidget *placeholder = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_valign(placeholder, GTK_ALIGN_CENTER);
  gtk_widget_set_halign(placeholder, GTK_ALIGN_CENTER);

  GtkWidget *title = gtk_label_new(_("Not signed in yet"));
  gtk_style_context_add_class(gtk_widget_get_style_context(title), "dim-label");
  PangoAttrList *attrs = pango_attr_list_new();
  pango_attr_list_insert(attrs, pango_attr_scale_new(1.5));
  gtk_label_set_attributes(GTK_LABEL(title), attrs);
  pango_attr_list_unref(attrs);
  gtk_box_pack_start(GTK_BOX(placeholder), title, FALSE, FALSE, 0);

  GtkWidget *subtitle = gtk_label_new(_("Login and chat arrive in a later phase."));
  gtk_style_context_add_class(gtk_widget_get_style_context(subtitle), "dim-label");
  gtk_box_pack_start(GTK_BOX(placeholder), subtitle, FALSE, FALSE, 0);

  gtk_container_add(GTK_CONTAINER(scrolled), placeholder);
  gtk_box_pack_start(GTK_BOX(box), scrolled, TRUE, TRUE, 0);

  /* Composer placeholder */
  GtkWidget *composer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  g_object_set(composer, "margin", 8, NULL);

  GtkWidget *entry = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(entry), _("Type a message…"));
  gtk_widget_set_sensitive(entry, FALSE);
  gtk_box_pack_start(GTK_BOX(composer), entry, TRUE, TRUE, 0);

  GtkWidget *send = gtk_button_new_with_label(_("Send"));
  gtk_widget_set_sensitive(send, FALSE);
  gtk_box_pack_start(GTK_BOX(composer), send, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(box), composer, FALSE, FALSE, 0);

  return box;
}

/* -------------------------------------------------------------------------- */
/* Construction                                                                */
/* -------------------------------------------------------------------------- */

static void
dataki_main_window_init(DatakiMainWindow *self)
{
  gtk_window_set_default_size(GTK_WINDOW(self), 1000, 700);
  gtk_window_set_title(GTK_WINDOW(self), "dataKI");

  gtk_window_set_titlebar(GTK_WINDOW(self), build_headerbar());

  GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_paned_set_position(GTK_PANED(paned), 260);
  gtk_paned_pack1(GTK_PANED(paned), build_sidebar(), FALSE, FALSE);
  gtk_paned_pack2(GTK_PANED(paned), build_chat_area(), TRUE, FALSE);
  gtk_container_add(GTK_CONTAINER(self), paned);

  g_signal_connect(self, "delete-event", G_CALLBACK(on_delete_event), NULL);

  gtk_widget_show_all(GTK_WIDGET(self));
}

static void
dataki_main_window_class_init(DatakiMainWindowClass *klass)
{
  (void)klass;
}

DatakiMainWindow *
dataki_main_window_new(GtkApplication *app)
{
  return g_object_new(DATAKI_TYPE_MAIN_WINDOW,
                      "application", app,
                      NULL);
}
