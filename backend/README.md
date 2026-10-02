# dataKI backend — native-client API (M2)

This folder contains the **server-side additions** that let the native dataKI
clients (Desktop/Android) authenticate and bootstrap against the existing
`dataKIWebUI_V4` backend, using a device-key Bearer token (Architecture
document §3 Option A, §5, §6).

Nothing here changes how the website itself works. Existing handlers
(`chat_handler.php`, `project_handler.php`, …) keep working unchanged — they
just additionally accept `Authorization: Bearer dk_…` because
`auth_current_user()` learns to resolve a device key.

## What's included

```
backend/
├─ dataKIWebUI_V4/                 ← drop these INTO your webui (same paths)
│  ├─ includes/client_auth.php     NEW  device-key auth, migration, login codes
│  ├─ api/v1/client/
│  │  ├─ info.php                  NEW  GET  server handshake (no auth)
│  │  ├─ bootstrap.php             NEW  GET  profile + models + limits (Bearer)
│  │  ├─ logout.php                NEW  POST revoke device key (Bearer)
│  │  └─ login/
│  │     ├─ start.php              NEW  GET  browser hand-off entry
│  │     └─ exchange.php           NEW  POST one-time code → device key
│  └─ .htaccess                    REPLACES root .htaccess (adds Authorization
│                                       passthrough; keeps existing protections)
└─ patches/                        ← tiny edits to two existing files
   ├─ includes__auth.php.patch
   └─ chat.php.patch
```

## Install

1. **Copy the new files** from `backend/dataKIWebUI_V4/` into your
   `dataKIWebUI_V4/` installation, keeping the same relative paths:
   - `includes/client_auth.php`
   - `api/v1/client/info.php`, `bootstrap.php`, `logout.php`,
     `login/start.php`, `login/exchange.php`

2. **Replace the root `.htaccess`** with the one provided (it keeps your
   existing `config.php`/`uploads_storage` protections and adds the
   Authorization passthrough so the Bearer header reaches root handlers such as
   `chat_handler.php`). If you prefer, just append the `Authorization` block.

3. **Apply the two patches** (from the `dataKIWebUI_V4/` root):

   ```sh
   patch -p1 < backend/patches/includes__auth.php.patch
   patch -p1 < backend/patches/chat.php.patch
   ```

   They do three small things:
   - `includes/auth.php`: load `client_auth.php`; in `auth_current_user()` accept
     a `Bearer dk_…` device key (any `AUTH_MODE`); in `auth_require_admin()`
     reject the device-key path (admin stays browser/session only).
   - `chat.php`: after a successful website login, return to the client
     hand-off endpoint (so the browser login completes back into the app).

4. **Database migration runs automatically** on first use (idempotent,
   `SHOW COLUMNS` / `ALTER TABLE`, same pattern as the existing migrations). It
   adds `kind, device_name, key_hash, expires_at, revoked_at, last_ip` to
   `server_keys` (and makes `api_key` nullable, since device keys store only a
   hash), and creates the `client_login_codes` table. No manual SQL needed.

## Security notes

- Device keys are `dk_` + 40 hex chars, returned **once** by `login/exchange`;
  the server stores only their **SHA-256 hash**.
- One-time login codes are single-use, hashed, and expire after 2 minutes.
- `redirect_uri` is validated (loopback `http://127.0.0.1:*` for desktop, or the
  WebUI's own https host with `/app/login/callback` for the Android App-Link) —
  open-redirect protection.
- A device key may do everything a normal chat session can, but **not** admin
  pages or account-security actions (those stay on the website).

## Endpoints

| Method · Path                         | Auth          | Purpose |
|---------------------------------------|---------------|---------|
| `GET  /api/v1/client/info.php`        | none          | version, chat_mode, api_level, upload limits |
| `GET  /api/v1/client/login/start.php` | none (browser)| browser hand-off; 302 → `redirect_uri?code=…&state=…` |
| `POST /api/v1/client/login/exchange.php` | code       | `{code}` → `{status, device_key, user}` |
| `GET  /api/v1/client/bootstrap.php`   | Bearer dk_…   | profile, token_status, models, effort, features, limits |
| `POST /api/v1/client/logout.php`      | Bearer dk_…   | revoke this device key |

Existing handlers (e.g. `POST /chat_handler.php`) also accept `Bearer dk_…`.

## Verified end-to-end

Tested against MariaDB 10.11 + PHP 8.3 (built-in server): `info` → website login
→ `login/start` (302 with code) → `login/exchange` (device key) → `bootstrap`
(full JSON) → `chat_handler.php?action=list_chats` with the Bearer
(`{"success":true,"chats":[]}`) → `logout` (revoked) → `bootstrap` again (401);
one-time code reuse rejected.

## Android App-Links (for the Android login)

The Android client returns from the browser via an https App-Link to
`/app/login/callback` (handled by `login/start.php`'s redirect validation). For
Android to open the app automatically, serve a Digital Asset Links file at

```
https://ai.datanet.ovh/.well-known/assetlinks.json
```

A template is included at `dataKIWebUI_V4/.well-known/assetlinks.json` — replace
`REPLACE_WITH_YOUR_APP_SIGNING_SHA256_FINGERPRINT` with your release (and debug,
if testing) signing-cert SHA-256 fingerprint:

```sh
keytool -list -v -keystore <your.keystore> -alias <alias> | grep SHA256
```

It must be served as `application/json` with no redirect. (The debug build uses
`ovh.datanet.dataki.client.debug`; add that package + its fingerprint too if you
test the debug variant.)
