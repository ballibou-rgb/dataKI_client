#include "tray.h"

/*
 * Windows tray backend using Shell_NotifyIcon (see docs/architecture.md
 * §ADR-2 / §9). A hidden message-only window receives the icon callbacks:
 *   - left click / double click  -> show the main window
 *   - right click                -> popup menu (Open / Quit)
 *
 * This file is only compiled for the Windows host; the GTK main loop on Windows
 * pumps the thread message queue, so our WndProc is dispatched normally.
 */

#include <windows.h>
#include <shellapi.h>
#include <glib.h>
#include <glib/gi18n.h>

#define DATAKI_TRAY_CALLBACK_MSG (WM_APP + 1)
#define DATAKI_TRAY_ICON_ID      1
#define DATAKI_MENU_SHOW         1001
#define DATAKI_MENU_QUIT         1002
#define DATAKI_WND_CLASS         L"DatakiTrayWindow"

struct _DatakiTray
{
  HWND               hwnd;
  NOTIFYICONDATAW    nid;
  DatakiTrayCallback on_show;
  DatakiTrayCallback on_quit;
  gpointer           user_data;
};

static void
show_context_menu(DatakiTray *self)
{
  POINT pt;
  GetCursorPos(&pt);

  HMENU menu = CreatePopupMenu();
  if (menu == NULL)
    return;

  g_autofree gunichar2 *open_w = g_utf8_to_utf16(_("Open"), -1, NULL, NULL, NULL);
  g_autofree gunichar2 *quit_w = g_utf8_to_utf16(_("Quit"), -1, NULL, NULL, NULL);

  AppendMenuW(menu, MF_STRING, DATAKI_MENU_SHOW,
              open_w ? (LPCWSTR)open_w : L"Open");
  AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
  AppendMenuW(menu, MF_STRING, DATAKI_MENU_QUIT,
              quit_w ? (LPCWSTR)quit_w : L"Quit");

  /* Required so the menu dismisses correctly on focus loss. */
  SetForegroundWindow(self->hwnd);

  int cmd = (int)TrackPopupMenu(menu,
                                TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                                pt.x, pt.y, 0, self->hwnd, NULL);
  DestroyMenu(menu);

  if (cmd == DATAKI_MENU_SHOW && self->on_show)
    self->on_show(self->user_data);
  else if (cmd == DATAKI_MENU_QUIT && self->on_quit)
    self->on_quit(self->user_data);
}

static LRESULT CALLBACK
tray_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  DatakiTray *self = (DatakiTray *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

  if (self != NULL && msg == DATAKI_TRAY_CALLBACK_MSG)
    {
      switch (LOWORD(lparam))
        {
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
          if (self->on_show)
            self->on_show(self->user_data);
          return 0;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
          show_context_menu(self);
          return 0;
        default:
          return 0;
        }
    }

  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static HICON
load_app_icon(HINSTANCE inst)
{
  /* Icon resource id 101 is embedded via packaging/windows/version.rc. */
  HICON icon = LoadIconW(inst, MAKEINTRESOURCEW(101));
  if (icon == NULL)
    icon = LoadIconW(NULL, IDI_APPLICATION);
  return icon;
}

DatakiTray *
dataki_tray_new(const char         *icon_name,
                DatakiTrayCallback  on_show,
                DatakiTrayCallback  on_quit,
                gpointer            user_data)
{
  (void)icon_name; /* Windows uses the embedded executable icon */

  HINSTANCE inst = GetModuleHandleW(NULL);

  WNDCLASSEXW wc = {0};
  wc.cbSize        = sizeof(wc);
  wc.lpfnWndProc   = tray_wnd_proc;
  wc.hInstance     = inst;
  wc.lpszClassName = DATAKI_WND_CLASS;
  /* Registering twice is harmless; ignore "already registered". */
  RegisterClassExW(&wc);

  DatakiTray *self = g_new0(DatakiTray, 1);
  self->on_show   = on_show;
  self->on_quit   = on_quit;
  self->user_data = user_data;

  self->hwnd = CreateWindowExW(0, DATAKI_WND_CLASS, L"dataKI",
                               0, 0, 0, 0, 0,
                               HWND_MESSAGE, NULL, inst, NULL);
  if (self->hwnd == NULL)
    {
      g_free(self);
      return NULL;
    }

  SetWindowLongPtrW(self->hwnd, GWLP_USERDATA, (LONG_PTR)self);

  self->nid.cbSize           = sizeof(self->nid);
  self->nid.hWnd             = self->hwnd;
  self->nid.uID              = DATAKI_TRAY_ICON_ID;
  self->nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  self->nid.uCallbackMessage = DATAKI_TRAY_CALLBACK_MSG;
  self->nid.hIcon            = load_app_icon(inst);
  wcsncpy(self->nid.szTip, L"dataKI", ARRAYSIZE(self->nid.szTip) - 1);

  if (!Shell_NotifyIconW(NIM_ADD, &self->nid))
    {
      DestroyWindow(self->hwnd);
      g_free(self);
      return NULL;
    }

  return self;
}

void
dataki_tray_free(DatakiTray *self)
{
  if (self == NULL)
    return;

  Shell_NotifyIconW(NIM_DELETE, &self->nid);
  if (self->nid.hIcon != NULL)
    DestroyIcon(self->nid.hIcon);
  if (self->hwnd != NULL)
    DestroyWindow(self->hwnd);

  g_free(self);
}
