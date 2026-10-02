#include "main_window.h"

#include <glib/gi18n.h>

#include "dataki-config.h"
#include "app.h"
#include "core/login.h"

struct _DatakiMainWindow
{
  GtkApplicationWindow parent_instance;

  GtkStack  *root_stack;    /* "login" <-> "main"            */
  GtkStack  *login_stack;   /* "idle"  <-> "waiting"         */
  GtkLabel  *login_error;   /* inline error on the login page */

  DatakiLogin *login;       /* in-flight login, or NULL       */
  char        *device_key;  /* current device key, or NULL    */
};

G_DEFINE_TYPE(DatakiMainWindow, dataki_main_window, GTK_TYPE_APPLICATION_WINDOW)

static const char *
server_url(void)
{
  const char *env = g_getenv("DATAKI_SERVER_URL");
  return (env != NULL && *env != '\0') ? env : DATAKI_SERVER_URL;
}

static void show_login_idle(DatakiMainWindow *self);
static void show_authenticated(DatakiMainWindow *self);

/* -------------------------------------------------------------------------- */
/* Close -> hide to tray                                                      */
/* -------------------------------------------------------------------------- */

static gboolean
on_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  (void)event;
  (void)user_data;
  GtkApplication *app = gtk_window_get_application(GTK_WINDOW(widget));
  if (DATAKI_IS_APP(app))
    return dataki_app_handle_window_close(DATAKI_APP(app));
  return FALSE;
}

/* -------------------------------------------------------------------------- */
/* Login flow                                                                 */
/* -------------------------------------------------------------------------- */

static void
on_login_done(gboolean ok, const char *error, const char *bootstrap_json, gpointer user_data)
{
  DatakiMainWindow *self = DATAKI_MAIN_WINDOW(user_data);

  g_clear_pointer(&self->login, dataki_login_cancel);

  if (ok)
    {
      (void)bootstrap_json; /* M4 parses models/token status from here */
      g_free(self->device_key);
      self->device_key = dataki_login_stored_key(server_url());
      show_authenticated(self);
    }
  else
    {
      gtk_label_set_text(self->login_error, error ? error : _("Sign-in failed."));
      show_login_idle(self);
    }
}

static void
start_login(DatakiMainWindow *self)
{
  gtk_label_set_text(self->login_error, "");
  gtk_stack_set_visible_child_name(self->login_stack, "waiting");

  g_autoptr(GError) error = NULL;
  self->login = dataki_login_start(server_url(), on_login_done, self, &error);
  if (self->login == NULL)
    {
      gtk_label_set_text(self->login_error,
                         error ? error->message : _("Could not start sign-in."));
      gtk_stack_set_visible_child_name(self->login_stack, "idle");
    }
}

static void
on_sign_in_clicked(GtkButton *button, gpointer user_data)
{
  (void)button;
  start_login(DATAKI_MAIN_WINDOW(user_data));
}

static void
on_cancel_login_clicked(GtkButton *button, gpointer user_data)
{
  (void)button;
  DatakiMainWindow *self = DATAKI_MAIN_WINDOW(user_data);
  g_clear_pointer(&self->login, dataki_login_cancel);
  show_login_idle(self);
}

static void
show_login_idle(DatakiMainWindow *self)
{
  gtk_stack_set_visible_child_name(self->login_stack, "idle");
  gtk_stack_set_visible_child_name(self->root_stack, "login");
}

static void
show_authenticated(DatakiMainWindow *self)
{
  gtk_stack_set_visible_child_name(self->root_stack, "main");
}

/* Startup auto-login result. */
static void
on_autologin_bootstrap(gboolean ok, long status, const char *bootstrap_json, gpointer user_data)
{
  DatakiMainWindow *self = DATAKI_MAIN_WINDOW(user_data);
  (void)bootstrap_json;

  if (ok)
    {
      show_authenticated(self);
    }
  else
    {
      /* 401 → stale key: forget it and ask to sign in. Network error → also
       * land on the login page (offline handling comes with chat in M4). */
      if (status == 401)
        {
          dataki_login_forget_key(server_url());
          g_clear_pointer(&self->device_key, g_free);
        }
      show_login_idle(self);
    }
}

void
dataki_main_window_try_autologin(DatakiMainWindow *self)
{
  g_free(self->device_key);
  self->device_key = dataki_login_stored_key(server_url());

  if (self->device_key != NULL)
    dataki_login_bootstrap_async(server_url(), self->device_key,
                                 on_autologin_bootstrap, self);
  else if (g_getenv("DATAKI_TEST_SIGNIN") != NULL)
    start_login(self); /* test hook: auto-start the browser hand-off */
  else
    show_login_idle(self);
}

void
dataki_main_window_logout(DatakiMainWindow *self)
{
  if (self->device_key != NULL)
    {
      dataki_login_logout_async(server_url(), self->device_key);
      g_clear_pointer(&self->device_key, g_free);
    }
  gtk_label_set_text(self->login_error, "");
  show_login_idle(self);
}

/* -------------------------------------------------------------------------- */
/* Header bar                                                                 */
/* -------------------------------------------------------------------------- */

static GtkWidget *
build_headerbar(void)
{
  GtkWidget *header = gtk_header_bar_new();
  gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
  gtk_header_bar_set_title(GTK_HEADER_BAR(header), "dataKI");

  GMenu *menu = g_menu_new();
  g_menu_append(menu, _("Sign out"), "app.logout");
  g_menu_append(menu, _("About dataKI"), "app.about");
  g_menu_append(menu, _("Quit"), "app.quit");

  GtkWidget *menu_button = gtk_menu_button_new();
  gtk_button_set_image(GTK_BUTTON(menu_button),
                       gtk_image_new_from_icon_name("open-menu-symbolic",
                                                    GTK_ICON_SIZE_BUTTON));
  gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_button), G_MENU_MODEL(menu));
  g_object_unref(menu);
  gtk_header_bar_pack_start(GTK_HEADER_BAR(header), menu_button);

  return header;
}

/* -------------------------------------------------------------------------- */
/* Login page                                                                  */
/* -------------------------------------------------------------------------- */

static GtkWidget *
build_login_idle(DatakiMainWindow *self)
{
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
  gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

  GtkWidget *logo = gtk_image_new_from_resource(
      "/ovh/datanet/dataki/client/icons/app-256.png");
  gtk_image_set_pixel_size(GTK_IMAGE(logo), 112);
  gtk_box_pack_start(GTK_BOX(box), logo, FALSE, FALSE, 0);

  GtkWidget *title = gtk_label_new(_("Welcome to dataKI"));
  PangoAttrList *attrs = pango_attr_list_new();
  pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
  pango_attr_list_insert(attrs, pango_attr_scale_new(1.7));
  gtk_label_set_attributes(GTK_LABEL(title), attrs);
  pango_attr_list_unref(attrs);
  gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

  GtkWidget *subtitle = gtk_label_new(_("Sign in with your browser to start chatting."));
  gtk_style_context_add_class(gtk_widget_get_style_context(subtitle), "dim-label");
  gtk_box_pack_start(GTK_BOX(box), subtitle, FALSE, FALSE, 0);

  GtkWidget *sign_in = gtk_button_new_with_label(_("Sign in"));
  gtk_style_context_add_class(gtk_widget_get_style_context(sign_in), "suggested-action");
  gtk_widget_set_halign(sign_in, GTK_ALIGN_CENTER);
  gtk_widget_set_size_request(sign_in, 220, 40);
  gtk_widget_set_margin_top(sign_in, 8);
  g_signal_connect(sign_in, "clicked", G_CALLBACK(on_sign_in_clicked), self);
  gtk_box_pack_start(GTK_BOX(box), sign_in, FALSE, FALSE, 0);

  self->login_error = GTK_LABEL(gtk_label_new(""));
  gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(self->login_error)),
                              "error");
  gtk_label_set_line_wrap(self->login_error, TRUE);
  gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(self->login_error), FALSE, FALSE, 0);

  GtkWidget *server = gtk_label_new("ai.datanet.ovh");
  gtk_style_context_add_class(gtk_widget_get_style_context(server), "dim-label");
  gtk_widget_set_margin_top(server, 18);
  gtk_box_pack_start(GTK_BOX(box), server, FALSE, FALSE, 0);

  return box;
}

static GtkWidget *
build_login_waiting(DatakiMainWindow *self)
{
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
  gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

  GtkWidget *spinner = gtk_spinner_new();
  gtk_widget_set_size_request(spinner, 48, 48);
  gtk_spinner_start(GTK_SPINNER(spinner));
  gtk_box_pack_start(GTK_BOX(box), spinner, FALSE, FALSE, 0);

  GtkWidget *label = gtk_label_new(_("Waiting for sign-in in your browser…"));
  gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

  GtkWidget *cancel = gtk_button_new_with_label(_("Cancel"));
  gtk_widget_set_halign(cancel, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top(cancel, 8);
  g_signal_connect(cancel, "clicked", G_CALLBACK(on_cancel_login_clicked), self);
  gtk_box_pack_start(GTK_BOX(box), cancel, FALSE, FALSE, 0);

  return box;
}

static GtkWidget *
build_login_page(DatakiMainWindow *self)
{
  self->login_stack = GTK_STACK(gtk_stack_new());
  gtk_stack_set_transition_type(self->login_stack, GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_stack_add_named(self->login_stack, build_login_idle(self), "idle");
  gtk_stack_add_named(self->login_stack, build_login_waiting(self), "waiting");
  gtk_stack_set_visible_child_name(self->login_stack, "idle");
  return GTK_WIDGET(self->login_stack);
}

/* -------------------------------------------------------------------------- */
/* Main page (placeholders until M4)                                          */
/* -------------------------------------------------------------------------- */

static GtkWidget *
build_main_page(void)
{
  GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_paned_set_position(GTK_PANED(paned), 260);

  GtkWidget *side = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_style_context_add_class(gtk_widget_get_style_context(side), "sidebar");
  g_object_set(side, "margin", 8, NULL);
  gtk_box_pack_start(GTK_BOX(side), gtk_button_new_with_label(_("New Chat")), FALSE, FALSE, 0);
  GtkWidget *search = gtk_search_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(search), _("Search chats…"));
  gtk_box_pack_start(GTK_BOX(side), search, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(side), gtk_scrolled_window_new(NULL, NULL), TRUE, TRUE, 0);
  gtk_paned_pack1(GTK_PANED(paned), side, FALSE, FALSE);

  GtkWidget *chat = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *chat_scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_set_vexpand(chat_scroll, TRUE);
  GtkWidget *hint = gtk_label_new(_("Signed in. Chat arrives in the next phase."));
  gtk_style_context_add_class(gtk_widget_get_style_context(hint), "dim-label");
  gtk_container_add(GTK_CONTAINER(chat_scroll), hint);
  gtk_box_pack_start(GTK_BOX(chat), chat_scroll, TRUE, TRUE, 0);
  GtkWidget *composer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  g_object_set(composer, "margin", 8, NULL);
  GtkWidget *entry = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(entry), _("Type a message…"));
  gtk_box_pack_start(GTK_BOX(composer), entry, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(composer), gtk_button_new_with_label(_("Send")), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(chat), composer, FALSE, FALSE, 0);
  gtk_paned_pack2(GTK_PANED(paned), chat, TRUE, FALSE);

  return paned;
}

/* -------------------------------------------------------------------------- */
/* Construction                                                                */
/* -------------------------------------------------------------------------- */

static void
dataki_main_window_finalize(GObject *object)
{
  DatakiMainWindow *self = DATAKI_MAIN_WINDOW(object);
  g_clear_pointer(&self->login, dataki_login_cancel);
  g_clear_pointer(&self->device_key, g_free);
  G_OBJECT_CLASS(dataki_main_window_parent_class)->finalize(object);
}

static void
dataki_main_window_init(DatakiMainWindow *self)
{
  gtk_window_set_default_size(GTK_WINDOW(self), 1000, 700);
  gtk_window_set_title(GTK_WINDOW(self), "dataKI");
  gtk_window_set_titlebar(GTK_WINDOW(self), build_headerbar());

  self->root_stack = GTK_STACK(gtk_stack_new());
  gtk_stack_add_named(self->root_stack, build_login_page(self), "login");
  gtk_stack_add_named(self->root_stack, build_main_page(), "main");
  gtk_stack_set_visible_child_name(self->root_stack, "login");
  gtk_container_add(GTK_CONTAINER(self), GTK_WIDGET(self->root_stack));

  g_signal_connect(self, "delete-event", G_CALLBACK(on_delete_event), NULL);

  gtk_widget_show_all(GTK_WIDGET(self));
}

static void
dataki_main_window_class_init(DatakiMainWindowClass *klass)
{
  G_OBJECT_CLASS(klass)->finalize = dataki_main_window_finalize;
}

DatakiMainWindow *
dataki_main_window_new(GtkApplication *app)
{
  return g_object_new(DATAKI_TYPE_MAIN_WINDOW, "application", app, NULL);
}
