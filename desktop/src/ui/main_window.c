#include "main_window.h"

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "dataki-config.h"
#include "app.h"
#include "core/login.h"
#include "core/chat.h"
#include "core/api.h"

struct _DatakiMainWindow
{
  GtkApplicationWindow parent_instance;

  GtkStack  *root_stack;
  GtkStack  *login_stack;
  GtkLabel  *login_error;

  DatakiLogin *login;
  char        *device_key;

  /* chat UI */
  GtkComboBoxText *model_combo;
  GtkLabel        *token_label;
  GtkListBox      *chat_list;
  GtkBox          *messages_box;
  GtkWidget       *msg_entry;
  GtkButton       *send_button;
  GtkScrolledWindow *chat_scroll;

  char     *effort_default;
  gboolean  is_proxy;

  /* current stream */
  DatakiChatRequest *chat_req;
  GtkLabel          *cur_content;
  GtkWidget         *cur_thinking_frame;
  GtkLabel          *cur_thinking;
  gint64             cur_chat_id;
  gboolean           streaming;
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
static void apply_bootstrap(DatakiMainWindow *self, const char *json);
static void load_chat_list(DatakiMainWindow *self);
static void start_send(DatakiMainWindow *self);
static void set_streaming(DatakiMainWindow *self, gboolean on);

/* -------------------------------------------------------------------------- */
/* Close -> tray                                                              */
/* -------------------------------------------------------------------------- */

static gboolean
on_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  (void)event; (void)user_data;
  GtkApplication *app = gtk_window_get_application(GTK_WINDOW(widget));
  if (DATAKI_IS_APP(app))
    return dataki_app_handle_window_close(DATAKI_APP(app));
  return FALSE;
}

/* -------------------------------------------------------------------------- */
/* Scrolling                                                                  */
/* -------------------------------------------------------------------------- */

static gboolean
scroll_to_bottom_idle(gpointer data)
{
  DatakiMainWindow *self = data;
  GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(self->chat_scroll);
  gtk_adjustment_set_value(adj, gtk_adjustment_get_upper(adj));
  return G_SOURCE_REMOVE;
}

static void
scroll_to_bottom(DatakiMainWindow *self)
{
  g_idle_add(scroll_to_bottom_idle, self);
}

/* -------------------------------------------------------------------------- */
/* Message bubbles                                                            */
/* -------------------------------------------------------------------------- */

static GtkWidget *
make_bubble(const char *css_class, const char *text, GtkLabel **out_label)
{
  GtkWidget *frame = gtk_frame_new(NULL);
  gtk_style_context_add_class(gtk_widget_get_style_context(frame), css_class);
  gtk_widget_set_halign(frame, g_strcmp0(css_class, "msg-user") == 0
                                   ? GTK_ALIGN_END : GTK_ALIGN_START);

  GtkWidget *label = gtk_label_new(text);
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_label_set_selectable(GTK_LABEL(label), TRUE);
  gtk_label_set_xalign(GTK_LABEL(label), 0.0);
  gtk_widget_set_margin_start(label, 10);
  gtk_widget_set_margin_end(label, 10);
  gtk_widget_set_margin_top(label, 6);
  gtk_widget_set_margin_bottom(label, 6);
  gtk_label_set_max_width_chars(GTK_LABEL(label), 90);
  gtk_container_add(GTK_CONTAINER(frame), label);

  if (out_label != NULL)
    *out_label = GTK_LABEL(label);
  return frame;
}

static void
append_user_message(DatakiMainWindow *self, const char *text)
{
  GtkWidget *b = make_bubble("msg-user", text, NULL);
  gtk_box_pack_start(self->messages_box, b, FALSE, FALSE, 0);
  gtk_widget_show_all(b);
  scroll_to_bottom(self);
}

/* Start a new assistant message; sets cur_content / cur_thinking. */
static void
begin_assistant_message(DatakiMainWindow *self)
{
  GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_halign(outer, GTK_ALIGN_START);

  /* Collapsible "thinking" area (hidden until the first thinking delta). */
  GtkWidget *exp = gtk_expander_new(_("Thinking"));
  GtkWidget *tlabel = gtk_label_new("");
  gtk_label_set_line_wrap(GTK_LABEL(tlabel), TRUE);
  gtk_label_set_xalign(GTK_LABEL(tlabel), 0.0);
  gtk_style_context_add_class(gtk_widget_get_style_context(tlabel), "dim-label");
  gtk_widget_set_margin_start(tlabel, 8);
  gtk_container_add(GTK_CONTAINER(exp), tlabel);
  gtk_widget_set_no_show_all(exp, TRUE); /* keep hidden until needed */
  gtk_box_pack_start(GTK_BOX(outer), exp, FALSE, FALSE, 0);

  GtkLabel *clabel = NULL;
  GtkWidget *bubble = make_bubble("msg-assistant", "", &clabel);
  gtk_box_pack_start(GTK_BOX(outer), bubble, FALSE, FALSE, 0);

  gtk_box_pack_start(self->messages_box, outer, FALSE, FALSE, 0);
  gtk_widget_show_all(outer);
  gtk_widget_hide(exp);

  self->cur_content        = clabel;
  self->cur_thinking_frame = exp;
  self->cur_thinking       = GTK_LABEL(tlabel);
  scroll_to_bottom(self);
}

static void
append_to_label(GtkLabel *label, const char *delta)
{
  const char *cur = gtk_label_get_text(label);
  g_autofree char *joined = g_strconcat(cur ? cur : "", delta, NULL);
  gtk_label_set_text(label, joined);
}

/* -------------------------------------------------------------------------- */
/* Streaming                                                                  */
/* -------------------------------------------------------------------------- */

static void
on_chat_event(const char *event_json, gpointer user_data)
{
  DatakiMainWindow *self = DATAKI_MAIN_WINDOW(user_data);

  g_autoptr(JsonParser) parser = json_parser_new();
  if (!json_parser_load_from_data(parser, event_json, -1, NULL))
    return;
  JsonNode *root = json_parser_get_root(parser);
  if (root == NULL || !JSON_NODE_HOLDS_OBJECT(root))
    return;
  JsonObject *obj = json_node_get_object(root);
  const char *type = json_object_has_member(obj, "type")
                       ? json_object_get_string_member(obj, "type") : "";

  if (g_strcmp0(type, "content_delta") == 0)
    {
      const char *d = json_object_has_member(obj, "delta")
                        ? json_object_get_string_member(obj, "delta") : "";
      if (self->cur_content) append_to_label(self->cur_content, d);
      scroll_to_bottom(self);
    }
  else if (g_strcmp0(type, "thinking_delta") == 0)
    {
      const char *d = json_object_has_member(obj, "delta")
                        ? json_object_get_string_member(obj, "delta") : "";
      if (self->cur_thinking_frame) gtk_widget_show(self->cur_thinking_frame);
      if (self->cur_thinking) append_to_label(self->cur_thinking, d);
    }
  else if (g_strcmp0(type, "chat_id") == 0)
    {
      if (json_object_has_member(obj, "chat_id"))
        self->cur_chat_id = json_object_get_int_member(obj, "chat_id");
    }
  else if (g_strcmp0(type, "error") == 0)
    {
      const char *m = json_object_has_member(obj, "message")
                        ? json_object_get_string_member(obj, "message") : _("Error");
      if (self->cur_content)
        {
          g_autofree char *e = g_strconcat("⚠ ", m, NULL);
          append_to_label(self->cur_content, e);
        }
    }
  else if (g_strcmp0(type, "done") == 0)
    {
      if (self->is_proxy && json_object_has_member(obj, "token_status"))
        {
          JsonObject *ts = json_object_get_object_member(obj, "token_status");
          if (ts && json_object_has_member(ts, "tier_used") &&
              json_object_has_member(ts, "limit"))
            {
              g_autofree char *s = g_strdup_printf("%" G_GINT64_FORMAT " / %" G_GINT64_FORMAT,
                  json_object_get_int_member(ts, "tier_used"),
                  json_object_get_int_member(ts, "limit"));
              gtk_label_set_text(self->token_label, s);
            }
        }
    }
}

static void
on_chat_done(const char *error, gpointer user_data)
{
  DatakiMainWindow *self = DATAKI_MAIN_WINDOW(user_data);

  if (error != NULL && self->cur_content != NULL)
    {
      g_autofree char *e = g_strconcat("⚠ ", error, NULL);
      append_to_label(self->cur_content, e);
    }

  g_clear_pointer(&self->chat_req, dataki_chat_request_free);
  self->cur_content = NULL;
  self->cur_thinking = NULL;
  self->cur_thinking_frame = NULL;
  set_streaming(self, FALSE);
}

static void
set_streaming(DatakiMainWindow *self, gboolean on)
{
  self->streaming = on;
  gtk_button_set_label(self->send_button, on ? _("Stop") : _("Send"));
  gtk_widget_set_sensitive(self->msg_entry, !on);
}

static void
start_send(DatakiMainWindow *self)
{
  const char *text = gtk_entry_get_text(GTK_ENTRY(self->msg_entry));
  if (text == NULL || *text == '\0')
    return;

  const char *model = gtk_combo_box_get_active_id(GTK_COMBO_BOX(self->model_combo));
  if (model == NULL)
    {
      gtk_label_set_text(self->token_label, _("No model"));
      return;
    }

  g_autofree char *content = g_strdup(text);
  gtk_entry_set_text(GTK_ENTRY(self->msg_entry), "");

  append_user_message(self, content);
  begin_assistant_message(self);

  DatakiChatParams params = {
    .content   = content,
    .chat_id   = self->cur_chat_id,
    .model     = model,
    .effort    = self->effort_default,
    .thinking  = TRUE,
    .websearch = FALSE,
  };

  self->chat_req = dataki_chat_send(server_url(), self->device_key, &params,
                                    on_chat_event, on_chat_done, self);
  set_streaming(self, TRUE);
}

static void
on_send_clicked(GtkButton *button, gpointer user_data)
{
  (void)button;
  DatakiMainWindow *self = DATAKI_MAIN_WINDOW(user_data);
  if (self->streaming)
    dataki_chat_cancel(self->chat_req);
  else
    start_send(self);
}

static void
on_entry_activate(GtkEntry *entry, gpointer user_data)
{
  (void)entry;
  DatakiMainWindow *self = DATAKI_MAIN_WINDOW(user_data);
  if (!self->streaming)
    start_send(self);
}

static void
on_new_chat_clicked(GtkButton *button, gpointer user_data)
{
  (void)button;
  DatakiMainWindow *self = DATAKI_MAIN_WINDOW(user_data);
  if (self->streaming)
    return;
  self->cur_chat_id = 0;
  gtk_container_foreach(GTK_CONTAINER(self->messages_box),
                        (GtkCallback)gtk_widget_destroy, NULL);
}

/* -------------------------------------------------------------------------- */
/* Chat list + history                                                        */
/* -------------------------------------------------------------------------- */

static void
render_history(DatakiMainWindow *self, const char *json)
{
  gtk_container_foreach(GTK_CONTAINER(self->messages_box),
                        (GtkCallback)gtk_widget_destroy, NULL);

  g_autoptr(JsonParser) parser = json_parser_new();
  if (!json_parser_load_from_data(parser, json, -1, NULL))
    return;
  JsonObject *obj = json_node_get_object(json_parser_get_root(parser));
  if (obj == NULL || !json_object_has_member(obj, "messages"))
    return;
  JsonArray *arr = json_object_get_array_member(obj, "messages");
  guint n = json_array_get_length(arr);
  for (guint i = 0; i < n; i++)
    {
      JsonObject *m = json_array_get_object_element(arr, i);
      const char *role = json_object_has_member(m, "role")
                           ? json_object_get_string_member(m, "role") : "user";
      const char *content = json_object_has_member(m, "content")
                              ? json_object_get_string_member(m, "content") : "";
      GtkWidget *b = make_bubble(g_strcmp0(role, "user") == 0 ? "msg-user" : "msg-assistant",
                                 content, NULL);
      gtk_box_pack_start(self->messages_box, b, FALSE, FALSE, 0);
      gtk_widget_show_all(b);
    }
  scroll_to_bottom(self);
}

static void
on_history_loaded(gboolean ok, long status, const char *json, gpointer user_data)
{
  (void)status;
  if (ok && json != NULL)
    render_history(DATAKI_MAIN_WINDOW(user_data), json);
}

static void
on_chat_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
  (void)box;
  DatakiMainWindow *self = DATAKI_MAIN_WINDOW(user_data);
  if (self->streaming || row == NULL)
    return;
  gint64 chat_id = (gint64)GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "chat-id"));
  self->cur_chat_id = chat_id;
  g_autofree char *path = g_strdup_printf("/chat_handler.php?action=get_messages&chat_id=%" G_GINT64_FORMAT, chat_id);
  dataki_api_get_async(server_url(), self->device_key, path, on_history_loaded, self);
}

static void
on_chat_list_loaded(gboolean ok, long status, const char *json, gpointer user_data)
{
  (void)status;
  DatakiMainWindow *self = DATAKI_MAIN_WINDOW(user_data);
  if (!ok || json == NULL)
    return;

  gtk_container_foreach(GTK_CONTAINER(self->chat_list),
                        (GtkCallback)gtk_widget_destroy, NULL);

  g_autoptr(JsonParser) parser = json_parser_new();
  if (!json_parser_load_from_data(parser, json, -1, NULL))
    return;
  JsonObject *obj = json_node_get_object(json_parser_get_root(parser));
  if (obj == NULL || !json_object_has_member(obj, "chats"))
    return;
  JsonArray *arr = json_object_get_array_member(obj, "chats");
  guint n = json_array_get_length(arr);
  for (guint i = 0; i < n; i++)
    {
      JsonObject *c = json_array_get_object_element(arr, i);
      gint64 id = json_object_has_member(c, "id") ? json_object_get_int_member(c, "id") : 0;
      const char *title = json_object_has_member(c, "title")
                            ? json_object_get_string_member(c, "title") : "Chat";

      GtkWidget *row = gtk_list_box_row_new();
      GtkWidget *lbl = gtk_label_new(title && *title ? title : _("New chat"));
      gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
      gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
      gtk_widget_set_margin_start(lbl, 6);
      gtk_widget_set_margin_end(lbl, 6);
      gtk_widget_set_margin_top(lbl, 6);
      gtk_widget_set_margin_bottom(lbl, 6);
      gtk_container_add(GTK_CONTAINER(row), lbl);
      g_object_set_data(G_OBJECT(row), "chat-id", GINT_TO_POINTER((int)id));
      gtk_list_box_insert(self->chat_list, row, -1);
      gtk_widget_show_all(row);
    }
}

static void
load_chat_list(DatakiMainWindow *self)
{
  dataki_api_get_async(server_url(), self->device_key,
                       "/chat_handler.php?action=list_chats",
                       on_chat_list_loaded, self);
}

/* -------------------------------------------------------------------------- */
/* Bootstrap                                                                  */
/* -------------------------------------------------------------------------- */

static gboolean
auto_send_idle(gpointer data)
{
  start_send(DATAKI_MAIN_WINDOW(data));
  return G_SOURCE_REMOVE;
}

static void
apply_bootstrap(DatakiMainWindow *self, const char *json)
{
  if (json == NULL)
    return;
  g_autoptr(JsonParser) parser = json_parser_new();
  if (!json_parser_load_from_data(parser, json, -1, NULL))
    return;
  JsonObject *obj = json_node_get_object(json_parser_get_root(parser));
  if (obj == NULL)
    return;

  const char *chat_mode = json_object_has_member(obj, "chat_mode")
                            ? json_object_get_string_member(obj, "chat_mode") : "proxy";
  self->is_proxy = (g_strcmp0(chat_mode, "proxy") == 0);

  g_free(self->effort_default);
  self->effort_default = json_object_has_member(obj, "effort_default")
                           ? g_strdup(json_object_get_string_member(obj, "effort_default"))
                           : g_strdup("mittel");

  /* models → combo */
  gtk_combo_box_text_remove_all(self->model_combo);
  if (json_object_has_member(obj, "models"))
    {
      JsonArray *models = json_object_get_array_member(obj, "models");
      guint n = json_array_get_length(models);
      for (guint i = 0; i < n; i++)
        {
          JsonObject *m = json_array_get_object_element(models, i);
          const char *id = json_object_has_member(m, "id")
                             ? json_object_get_string_member(m, "id") : NULL;
          const char *label = json_object_has_member(m, "label")
                                ? json_object_get_string_member(m, "label") : id;
          if (id != NULL)
            gtk_combo_box_text_append(self->model_combo, id, label);
        }
      if (n > 0)
        gtk_combo_box_set_active(GTK_COMBO_BOX(self->model_combo), 0);
    }
  gtk_widget_show(GTK_WIDGET(self->model_combo));

  /* token footer (proxy only) */
  if (self->is_proxy && json_object_has_member(obj, "token_status"))
    {
      JsonObject *ts = json_object_get_object_member(obj, "token_status");
      if (ts && json_object_has_member(ts, "tier"))
        {
          gtk_label_set_text(self->token_label,
                             json_object_get_string_member(ts, "tier"));
          gtk_widget_show(GTK_WIDGET(self->token_label));
        }
    }

  load_chat_list(self);

  /* test hook: auto-send a message once authenticated */
  const char *tm = g_getenv("DATAKI_TEST_MESSAGE");
  if (tm != NULL && *tm != '\0')
    {
      gtk_entry_set_text(GTK_ENTRY(self->msg_entry), tm);
      g_idle_add(auto_send_idle, self);
    }
}

/* -------------------------------------------------------------------------- */
/* Login glue                                                                 */
/* -------------------------------------------------------------------------- */

static void
on_login_done(gboolean ok, const char *error, const char *bootstrap_json, gpointer user_data)
{
  DatakiMainWindow *self = DATAKI_MAIN_WINDOW(user_data);
  g_clear_pointer(&self->login, dataki_login_cancel);

  if (ok)
    {
      g_free(self->device_key);
      self->device_key = dataki_login_stored_key(server_url());
      apply_bootstrap(self, bootstrap_json);
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
      gtk_label_set_text(self->login_error, error ? error->message : _("Could not start sign-in."));
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

static void
on_autologin_bootstrap(gboolean ok, long status, const char *bootstrap_json, gpointer user_data)
{
  DatakiMainWindow *self = DATAKI_MAIN_WINDOW(user_data);
  if (ok)
    {
      apply_bootstrap(self, bootstrap_json);
      show_authenticated(self);
    }
  else
    {
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
  if (self->device_key == NULL)
    {
      const char *tk = g_getenv("DATAKI_TEST_KEY"); /* test hook: skip login */
      if (tk != NULL)
        self->device_key = g_strdup(tk);
    }

  if (self->device_key != NULL)
    dataki_login_bootstrap_async(server_url(), self->device_key, on_autologin_bootstrap, self);
  else if (g_getenv("DATAKI_TEST_SIGNIN") != NULL)
    start_login(self);
  else
    show_login_idle(self);
}

void
dataki_main_window_logout(DatakiMainWindow *self)
{
  if (self->streaming)
    dataki_chat_cancel(self->chat_req);
  if (self->device_key != NULL)
    {
      dataki_login_logout_async(server_url(), self->device_key);
      g_clear_pointer(&self->device_key, g_free);
    }
  gtk_label_set_text(self->login_error, "");
  show_login_idle(self);
}

/* -------------------------------------------------------------------------- */
/* Header + pages                                                             */
/* -------------------------------------------------------------------------- */

static GtkWidget *
build_headerbar(DatakiMainWindow *self)
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
                       gtk_image_new_from_icon_name("open-menu-symbolic", GTK_ICON_SIZE_BUTTON));
  gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_button), G_MENU_MODEL(menu));
  g_object_unref(menu);
  gtk_header_bar_pack_start(GTK_HEADER_BAR(header), menu_button);

  self->model_combo = GTK_COMBO_BOX_TEXT(gtk_combo_box_text_new());
  gtk_widget_set_no_show_all(GTK_WIDGET(self->model_combo), TRUE);
  gtk_header_bar_pack_start(GTK_HEADER_BAR(header), GTK_WIDGET(self->model_combo));

  self->token_label = GTK_LABEL(gtk_label_new(""));
  gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(self->token_label)), "dim-label");
  gtk_widget_set_no_show_all(GTK_WIDGET(self->token_label), TRUE);
  gtk_header_bar_pack_end(GTK_HEADER_BAR(header), GTK_WIDGET(self->token_label));

  return header;
}

static GtkWidget *
build_login_idle(DatakiMainWindow *self)
{
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
  gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

  GtkWidget *logo = gtk_image_new_from_resource("/ovh/datanet/dataki/client/icons/app-256.png");
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
  gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(self->login_error)), "error");
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
  gtk_box_pack_start(GTK_BOX(box), gtk_label_new(_("Waiting for sign-in in your browser…")), FALSE, FALSE, 0);
  GtkWidget *cancel = gtk_button_new_with_label(_("Cancel"));
  gtk_widget_set_halign(cancel, GTK_ALIGN_CENTER);
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

static GtkWidget *
build_main_page(DatakiMainWindow *self)
{
  GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_paned_set_position(GTK_PANED(paned), 260);

  /* sidebar */
  GtkWidget *side = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_style_context_add_class(gtk_widget_get_style_context(side), "sidebar");
  g_object_set(side, "margin", 8, NULL);
  GtkWidget *new_chat = gtk_button_new_with_label(_("New Chat"));
  g_signal_connect(new_chat, "clicked", G_CALLBACK(on_new_chat_clicked), self);
  gtk_box_pack_start(GTK_BOX(side), new_chat, FALSE, FALSE, 0);
  GtkWidget *list_scroll = gtk_scrolled_window_new(NULL, NULL);
  self->chat_list = GTK_LIST_BOX(gtk_list_box_new());
  g_signal_connect(self->chat_list, "row-activated", G_CALLBACK(on_chat_row_activated), self);
  gtk_container_add(GTK_CONTAINER(list_scroll), GTK_WIDGET(self->chat_list));
  gtk_box_pack_start(GTK_BOX(side), list_scroll, TRUE, TRUE, 0);
  gtk_paned_pack1(GTK_PANED(paned), side, FALSE, FALSE);

  /* chat area */
  GtkWidget *chat = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  self->chat_scroll = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL, NULL));
  gtk_widget_set_vexpand(GTK_WIDGET(self->chat_scroll), TRUE);
  self->messages_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 10));
  g_object_set(self->messages_box, "margin", 12, NULL);
  gtk_container_add(GTK_CONTAINER(self->chat_scroll), GTK_WIDGET(self->messages_box));
  gtk_box_pack_start(GTK_BOX(chat), GTK_WIDGET(self->chat_scroll), TRUE, TRUE, 0);

  GtkWidget *composer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  g_object_set(composer, "margin", 8, NULL);
  self->msg_entry = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(self->msg_entry), _("Type a message…"));
  g_signal_connect(self->msg_entry, "activate", G_CALLBACK(on_entry_activate), self);
  gtk_box_pack_start(GTK_BOX(composer), self->msg_entry, TRUE, TRUE, 0);
  self->send_button = GTK_BUTTON(gtk_button_new_with_label(_("Send")));
  g_signal_connect(self->send_button, "clicked", G_CALLBACK(on_send_clicked), self);
  gtk_box_pack_start(GTK_BOX(composer), GTK_WIDGET(self->send_button), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(chat), composer, FALSE, FALSE, 0);

  gtk_paned_pack2(GTK_PANED(paned), chat, TRUE, FALSE);
  return paned;
}

/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

static void
dataki_main_window_finalize(GObject *object)
{
  DatakiMainWindow *self = DATAKI_MAIN_WINDOW(object);
  if (self->chat_req != NULL)
    {
      dataki_chat_cancel(self->chat_req);
      dataki_chat_request_free(self->chat_req);
    }
  g_clear_pointer(&self->login, dataki_login_cancel);
  g_clear_pointer(&self->device_key, g_free);
  g_clear_pointer(&self->effort_default, g_free);
  G_OBJECT_CLASS(dataki_main_window_parent_class)->finalize(object);
}

static void
dataki_main_window_init(DatakiMainWindow *self)
{
  gtk_window_set_default_size(GTK_WINDOW(self), 1000, 700);
  gtk_window_set_title(GTK_WINDOW(self), "dataKI");
  gtk_window_set_titlebar(GTK_WINDOW(self), build_headerbar(self));

  self->root_stack = GTK_STACK(gtk_stack_new());
  gtk_stack_add_named(self->root_stack, build_login_page(self), "login");
  gtk_stack_add_named(self->root_stack, build_main_page(self), "main");
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
