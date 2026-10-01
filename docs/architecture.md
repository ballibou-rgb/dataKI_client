# dataKI Native Client (C + GTK3, Linux + Windows) — Arbeits-, Planungs- und Architekturdokument

| | |
|---|---|
| Status | Entwurf 0.3 · Stand 2026-10-01 · feste Server-URL + Browser-Handoff-Login + Linux/Windows-Build mit dt. NSIS-Installer entschieden |
| Bezug | dataKIWebUI 8.0.0 (`dataKIWebUI_V4`) |
| Zielgruppe | GUI-Programmierer (C/GTK3, Linux + Windows) und Backend-Entwickler (PHP) |
| Sprache der Doku | Deutsch · Code, Bezeichner, Commits: Englisch |

**Kennzeichnung im Dokument:**
- **[IST]** = aus dem vorhandenen Code gelesen (Dateinamen genannt).
- **[SOLL]** = Vorschlag/Anforderung, neu zu bauen.
- **[PRÜFEN]** = beim Umsetzen im Code zu verifizieren, wurde bei der Analyse nicht im Detail gelesen.

---

## 1. Ziel und Abgrenzung

### 1.1 Ziel
Eine **native Desktop-Anwendung** (C + GTK3, Zielplattformen **Linux und Windows**, siehe 14/15 #7), die dieselben Chat-Funktionen wie die WebUI bietet — der eigentliche Chat läuft **ohne Browser**. Die Server-URL ist in v1 **fest auf `ai.datanet.ovh`** eingestellt (kein Eingabefeld im Client); „eigenen Server hinzufügen" ist ein späteres Soll-Feature (siehe 1.3/1.4). Die **Anmeldung läuft immer über die Website** im System-Browser (Abschnitt 5) — Passwort, 2FA, Captcha, Sperren und Google-Login bleiben vollständig Sache der Website. Danach laufen **Key-Verwaltung, Modellliste, Verlauf und Token-Stand automatisch**, ohne erneuten Browser-Aufruf.

### 1.2 Muss (MVP)
- Anmeldung per **Browser-Handoff** gegen die feste Server-URL `ai.datanet.ovh` (Abschnitt 5) — die Website übernimmt E-Mail/Passwort, 2FA, Captcha, Sperr-/Fehlerzustände und Google-Login; der Client sieht nur „erfolgreich" oder „abgebrochen/fehlgeschlagen".
- Automatische Key-Erzeugung und sichere Ablage (Keyring) — kein manuelles Kopieren von Keys.
- Chat mit **Streaming**, Modellwahl, „Denken"-Anzeige, Abbrechen.
- Chat-Liste, Verlauf laden, löschen, umbenennen/suchen.
- Markdown- und Codeblock-Darstellung.
- Funktioniert in **beiden** Server-Betriebsmodi (`proxy` und `direct`).
- Läuft auf **Linux und Windows**; Auslieferung als `.deb`/AppImage (Linux) sowie Windows-App + **NSIS-Installer auf Deutsch** (Windows) — siehe Abschnitt 14.

### 1.3 Soll (spätere Phasen)
**Eigenen Server hinzufügen** (konfigurierbare Server-URL statt fest `ai.datanet.ovh`), Projekte, Datei-Anhänge, Wissensbasen (RAG), Tools, Deep Research, Bildgenerierung, Sprache (STT/TTS), Code-Ausführung, Canvas-Vorschau, Tray/Benachrichtigungen.

### 1.4 Nicht-Ziele (v1)
- **Admin-Panel** bleibt im Browser (Link „Im Browser öffnen").
- **Konfigurierbare Server-URL:** v1 spricht ausschließlich mit `ai.datanet.ovh`; „eigenen Server hinzufügen" ist Soll (1.3).
- **Registrierung, Passwort-Reset, Token-Kauf (Stripe), 2FA-Einrichtung, Login selbst** laufen **alle** per **Browser-Handoff** (System-Browser öffnen) — es gibt keinen nativen Login/2FA/Captcha-Dialog im Client (siehe Abschnitt 5).
- Keine lokale Chat-Datenbank: **der Server ist die einzige Quelle der Wahrheit** (kein Sync-Problem).
- macOS: nicht v1 (Windows **ist** v1-Ziel, siehe Abschnitt 14 und 15 #7).

---

## 2. Ist-Zustand des Backends (relevant für die GUI)

### 2.1 Zwei Betriebsmodi `[IST: config.php, includes/functions.php]`
| `CHAT_MODE` | Verhalten | Folgen für die GUI |
|---|---|---|
| `proxy` | Chat läuft über den externen **ollama-proxy**. Tier, Token-Stand, Pakete, RAG, Bild, Sprache, Tools, Research, Sandbox kommen vom Proxy. | Token-Anzeige, Pro-Hinweise, alle Zusatzfunktionen möglich |
| `direct` | WebUI spricht direkt mit Ollama/vLLM-Backends. Kein Tier/Token/Kauf. Login optional (`direct_login_required`), sonst Gast-Konten. | Token-/Pro-UI ausblenden, Zusatzfunktionen meist nicht verfügbar |

`AUTH_MODE` (standalone/proxy) ist davon unabhängig und betrifft nur die Anmeldung an der WebUI.

### 2.2 Authentifizierung heute `[IST]`
- Login per **PHP-Session-Cookie** (`login_handler.php` → `auth_attempt_login()`), Cookie `chatapp_session`, 7 Tage, `SameSite=Lax`.
- Ergebnisstatus von `auth_attempt_login()`: `ok | invalid | locked | just_locked | needs_2fa | unverified | inactive`.
- **Captcha** nach N Fehlversuchen (Session-Zähler), Sperre des Kontos nach 6 (`security_thresholds()`).
- **2FA** ist ein HTML-Formular (`login_2fa.php`), Zwischenzustand liegt in `$_SESSION['pending_2fa_user']`.
- **Google-OAuth** läuft browserbasiert (`auth_google.php`, `auth_google_callback.php`).
- **Es gibt keinen Token-/Key-basierten Client-Login.** Alle Handler nutzen `auth_current_user()` (Session).

### 2.3 Zwei verschiedene „Keys" — nicht verwechseln `[IST]`
| Key | Wo | Wer sieht ihn | Zweck |
|---|---|---|---|
| `users.proxy_api_key` (`sk-…`) | WebUI-DB, vom ollama-proxy erzeugt | **nur der WebUI-Server** | Bearer-Auth der WebUI gegenüber dem Proxy |
| `x-admin-key` | `config.local.php` | **nur der WebUI-Server** | Admin-Endpunkte des Proxys |
| `server_keys.api_key` | WebUI-DB | Nutzer (Dashboard) | Zugriff auf `api/v1/completions.php` |

> **Harte Regel:** Der native Client bekommt **niemals** `proxy_api_key` oder `x-admin-key`. Er kennt nur einen **eigenen Geräte-Key** (Abschnitt 5), den der WebUI-Server intern auf den Nutzer und dessen Proxy-Key abbildet.

### 2.4 Vorhandene Schnittstellen, die der Client nutzen kann `[IST]`
| Funktion | Datei | Auth heute |
|---|---|---|
| Chat senden (SSE), Liste, Verlauf, Suche, Export, Bearbeiten, Verzweigen, Löschen | `chat_handler.php` | Session |
| Projekte | `project_handler.php` | Session |
| Wissensbasen | `collection_handler.php` | Session |
| Tools an/aus | `tool_handler.php` | Session |
| Deep Research (start/status) | `research_handler.php` | Session |
| Code ausführen | `code_handler.php` | Session |
| Bild, Sprache, Gedächtnis | `image_handler.php`, `voice_handler.php`, `memory_handler.php` | Session |
| Dateien (Liste/Löschen), Datei-Aktionen | `file_list_handler.php`, `file_actions_handler.php` | Session |
| Datei-Download | `file.php` `[PRÜFEN]` | Session |
| Feedback 👍/👎 | `feedback_handler.php` | Session |
| OpenAI-kompatibles Gateway (nur Text) | `api/v1/completions.php` | `Authorization: Bearer <server_key>` |

Antwortformat der Handler: JSON mit `success`, Fehler über `json_error()`.

### 2.5 Was die Chat-Seite beim Laden serverseitig mitgibt `[IST]`
Modellliste (`get_chat_models()`), Aufwandsstufen (`get_effort_levels()`), Standard-Aufwand, Datei-Aktionen, Plus-Menü, Chat-UI-Konfiguration (`get_chat_ui_config()` `[PRÜFEN]`). **Dafür existiert kein JSON-Endpunkt** — der Client braucht einen (siehe 6.3).

---

## 3. Architekturentscheidungen (ADR)

### ADR-1 · Kommunikationspfad: **nur über den WebUI-Server**
| Option | Beschreibung | Bewertung |
|---|---|---|
| **A (empfohlen)** | Client → WebUI-Server. Neuer Geräte-Key-Login + `auth_current_user()` akzeptiert zusätzlich `Authorization: Bearer <Geräte-Key>`. Alle bestehenden Handler laufen damit **unverändert** auch für den Client. | wenig Backend-Aufwand, gleiche Logik wie Web, 2FA/Sperren/Verlauf/Projekte/RAG bleiben erhalten |
| B | Neue, saubere REST-Schicht `api/v1/client/*` für alles | sauberer/versioniert, aber deutlich mehr Aufwand und doppelte Logik |
| C | Client direkt gegen den ollama-proxy | **verworfen:** Chat-Verlauf, Projekte, Sammlungen, 2FA, Sperrlogik liegen in der WebUI-DB; der Nutzer-Key müsste auf den Client |

**Entscheidung:** Option A. Neue Endpunkte nur dort, wo es heute keinen JSON-Weg gibt (Login, Info, Bootstrap).

### ADR-2 · Sprache/Toolkit: **C + GTK3** (Linux und Windows)
Rust (gtk-rs, reqwest, serde, `keyring`-Crate) wäre gleichwertig möglich — Architektur und Backend-Vertrag in diesem Dokument sind sprachunabhängig. Hinweis: GTK3 ist im Wartungsmodus (GTK4/libadwaita ist aktuell). Für den Linux-Client ist GTK3 weiterhin tragfähig; die **UI-Schicht strikt von der Logik trennen** (Abschnitt 9), damit ein späterer Wechsel auf GTK4 nur die UI betrifft. **Windows:** GTK3 läuft über das MSYS2/mingw-w64-Ökosystem (`mingw-w64-x86_64-gtk3`, `-gtksourceview3`, `-curl`, `-json-glib`); Build entweder nativ unter MSYS2 oder per Cross-Compile von Linux aus mit `x86_64-w64-mingw32-gcc`. GTK3-Look unter Windows wirkt „fremd" (kein natives Theme) — akzeptiert für v1, ggf. später GTK4/libadwaita oder Rust+Tauri als Alternative prüfen.

### ADR-3 · Markdown-Darstellung: **md4c → Widget-Baum** (kein Web-Engine-Zwang)
- Antworten sind Markdown (Absätze, Listen, Tabellen, Codeblöcke, Links).
- Empfehlung: **md4c** parst, daraus wird je Block ein GTK-Widget (Absatz = `GtkTextView`/Label, Codeblock = `GtkSourceView` mit Kopieren-Button, Tabelle = `GtkGrid`).
- Alternative: **WebKit2GTK** für die Nachrichtenliste (schnell gebaut, Tabellen/Formeln leicht, aber schwerer und „nicht mehr nativ"). **WebKit nur optional** für die spätere Canvas-Vorschau (HTML/SVG) — unter Windows ohnehin nicht verfügbar, dort entfällt die Canvas-Vorschau bzw. wird über einen externen Browser-Aufruf gelöst.
- Beim Streaming wird der **letzte Block inkrementell** neu gerendert, nicht die ganze Liste.

### ADR-4 · Geheimnisse: plattformabhängiger Secret-Store
Passwort wird **nie gespeichert**. Gespeichert wird nur der Geräte-Key, hinter einer gemeinsamen `SecretStore`-Schnittstelle (9.2) mit zwei Implementierungen:
- **Linux:** `libsecret` (Secret Service / GNOME Keyring / KWallet). Fallback ohne Secret Service: Datei mit `0600` plus deutlicher Warnung im UI (oder Abbruch — siehe Offene Entscheidungen).
- **Windows:** **Windows Credential Manager** (`CredWriteW`/`CredReadW`/`CredDeleteW` aus `Advapi32`), Zielname z. B. `dataKI:ai.datanet.ovh`. Kein Secret-Service-Äquivalent nötig, da Credential Manager immer verfügbar ist.

---

## 4. Soll-Architektur

```
┌──────────────────────────── dataKI Native Client (C/GTK3) ───────────────────────────┐
│                                                                                       │
│  UI-Schicht (GTK3, nur Darstellung)                                                   │
│   LoginDialog · MainWindow · Sidebar · ChatView · MessageWidget · Composer · Settings │
│        │  Signale/Callbacks (GObject)                       ▲ g_idle_add / invoke     │
│  ──────▼────────────────────────────────────────────────────┴───────────────────────  │
│  Logik-Schicht (kein GTK-Include)                                                     │
│   AuthManager (Zustandsautomat) · ChatService · ModelService · FileService · …        │
│        │                                                                              │
│  ──────▼──────────────────────────────────────────────────────────────────────────  │
│  Transport                                                                            │
│   HttpClient (libcurl, Worker-Threads) · SseParser · JSON (json-glib)                │
│  Plattform                                                                            │
│   SecretStore (libsecret) · Config (GKeyFile, XDG) · Logger                           │
└───────────────────────────────────────────┬───────────────────────────────────────────┘
                                            │ HTTPS, Bearer <Geräte-Key>
                                            ▼
                    ┌──────────────── dataKIWebUI (PHP) ────────────────┐
                    │ /api/v1/client/info · login/start (Browser) ·     │
                    │ login/exchange · logout                          │
                    │ /api/v1/client/bootstrap                          │
                    │ chat_handler.php · project_handler.php · …        │  ← unverändert,
                    │ auth_current_user(): Session ODER Bearer          │     nur Auth erweitert
                    └───────────────┬───────────────────────────────────┘
                                    │ proxy_api_key (nur serverseitig)
                                    ▼
                             ollama-proxy  /  Ollama · vLLM (direct)
```

---

## 5. Login- und Key-Management („automatisch")

### 5.1 Prinzip — Login ausschließlich per Browser-Handoff
Es gibt **keine** native Eingabe von Server-URL, E-Mail, Passwort, 2FA-Code oder Captcha im Client. Die Website ist immer die alleinige Auth-Instanz (analog zum bestehenden Google-OAuth-Muster `[IST: auth_google.php]`, hier aber für **jeden** Login-Weg):
1. Client zeigt nur einen Knopf „Anmelden" (feste Server-URL `ai.datanet.ovh`, Abschnitt 1.1).
2. Client startet einen lokalen Loopback-Listener (`127.0.0.1:<zufälliger Port>`) und öffnet den **System-Browser** auf `GET /api/v1/client/login/start?...&redirect_uri=http://127.0.0.1:<port>/callback&state=<zufällig>`.
3. Nutzer meldet sich auf der Website an — mit **allem, was die Website dafür ohnehin anbietet** (Passwort, 2FA, Captcha, Sperrlogik, Google-Login, Passwort-vergessen …). Der Client bekommt davon nichts mit und muss keinen dieser Fälle selbst abbilden.
4. Nach Erfolg leitet die Website auf `redirect_uri` mit einem kurzlebigen **Einmal-Code** um; der lokale Listener fängt ihn ab, der Client tauscht ihn serverseitig gegen einen **Geräte-Key** (`POST /api/v1/client/login/exchange`).
5. Der Client legt den Geräte-Key im Keyring ab. Es gibt nie ein Passwort oder einen 2FA-Code im Prozessspeicher des Clients.
6. Ab dann: Start → Key aus Keyring → `GET bootstrap` → direkt im Chat. Kein erneuter Browser-Aufruf, bis der Key widerrufen wird oder der Nutzer sich abmeldet.

### 5.2 Zustandsautomat der GUI
```
NEEDS_LOGIN ──„Anmelden" geklickt──► WAITING_FOR_BROWSER (Loopback-Listener offen, Browser geöffnet)
WAITING_FOR_BROWSER ──Callback mit code & korrektem state──► EXCHANGING (POST login/exchange)
WAITING_FOR_BROWSER ──Abbruch/Fenster geschlossen/Timeout (~5 min)──► NEEDS_LOGIN
EXCHANGING ──ok──► AUTHENTICATED
EXCHANGING ──Fehler──► NEEDS_LOGIN (Meldung „Anmeldung fehlgeschlagen, erneut versuchen")
AUTHENTICATED ──irgendein Request 401──► NEEDS_LOGIN (Key im Keyring löschen)
AUTHENTICATED ──Netzfehler──► bleibt AUTHENTICATED, Offline-Banner + Retry (Key NICHT löschen)
```
Captcha, 2FA, Sperren, „unverified", „must_change_password" etc. tauchen in diesem Automaten **nicht mehr auf** — sie sind reine Website-Zustände und werden dort angezeigt, während der Client nur in `WAITING_FOR_BROWSER` verharrt.

### 5.3 Start-Ablauf (automatisch)
```
start
 ├─ SecretStore.lookup(server="ai.datanet.ovh")
 │    ├─ kein Key → NEEDS_LOGIN (Button „Anmelden")
 │    └─ Key → GET /api/v1/client/bootstrap
 │           ├─ 200 → MainWindow (Modelle, Aufwand, Token-Stand, Chat-Liste laden)
 │           ├─ 401 → Key löschen → NEEDS_LOGIN
 │           └─ Netzfehler → MainWindow im Offline-Modus + Retry
```

### 5.4 Geräte-Key `[SOLL]`
- Format: `dk_` + 40 Hex-Zeichen (`random_bytes(20)`). Wird **nur einmal** im `login/exchange`-Response ausgeliefert.
- Server speichert nur den **SHA-256-Hash**. (Hinweis `[IST]`: die bestehenden `server_keys.api_key` liegen im Klartext in der DB — für die neuen Geräte-Keys bitte nicht übernehmen.)
- Ablage in `server_keys` (Erweiterung per Auto-Migration, siehe 6.1): `kind='device'`, `device_name` (z. B. „Desktop · hostname"), `key_hash`, `expires_at` (optional), `revoked_at`, `last_ip`.
- Der Nutzer sieht und widerruft Geräte im **Dashboard** (dort werden `server_keys` bereits verwaltet `[IST: server_keys_handler.php, dashboard.php]` — Anzeige der Geräte-Keys ergänzen).
- **Rechte-Scope:** Ein Geräte-Key darf alles, was die normale Chat-Session darf, aber **nicht**: Admin-Seiten (`auth_require_admin()` verlangt weiterhin Session), Passwort ändern, 2FA ein/aus, Konto löschen. Diese laufen im Browser.

### 5.5 Fehler- und Sonderfälle (alles Website-seitig)
| Fall | Verhalten |
|---|---|
| Login auf der Website fehlgeschlagen/abgebrochen (invalid, captcha, 2FA, locked, unverified, …) | Website zeigt es wie gewohnt; Client bleibt einfach in `WAITING_FOR_BROWSER`, bis Erfolg oder Timeout |
| Nutzer schließt den Browser-Tab ohne Login | Client-Timeout (~5 min) → zurück zu `NEEDS_LOGIN` |
| `state`-Mismatch im Callback | Code verwerfen, `NEEDS_LOGIN` + Fehlermeldung (CSRF-Schutz) |
| Code bereits eingelöst / abgelaufen | `login/exchange` liefert Fehler → `NEEDS_LOGIN` |
| HTTP 429 beim Exchange | Wartezeit anzeigen (Server soll `Retry-After` senden) |

---

## 6. Backend-Arbeitspaket (PHP) — Voraussetzung für die GUI

### 6.1 Auth-Erweiterung `[SOLL]`
1. **Migration** (nach dem vorhandenen Auto-Migrations-Muster, `SHOW COLUMNS … LIKE` / `ALTER TABLE`): `server_keys` um `kind`, `device_name`, `key_hash`, `expires_at`, `revoked_at`, `last_ip` erweitern.
2. **`auth_current_user()`** (`includes/auth.php`): wenn keine Session-User vorhanden ist und ein `Authorization: Bearer dk_…` anliegt → Hash nachschlagen, `revoked_at`/`expires_at`/`is_active` prüfen, Nutzer zurückgeben, `last_used_at` aktualisieren. Dabei Flag setzen (`$GLOBALS['auth_via_device_key']=true`), damit `auth_require_admin()` und Konto-Sicherheitsaktionen diesen Pfad **ablehnen**.
3. **Authorization-Header-Durchreichung** (`.htaccess`): bisher nur in `api/v1/.htaccess` `[IST]`. Die Regeln (`RewriteRule … E=HTTP_AUTHORIZATION`, `SetEnvIfNoCase`) auch im **Projekt-Root** ergänzen, sonst kommen Bearer-Header bei Apache+FPM/CGI nicht an. Der Bearer-Parser `api_bearer_token()` aus `api/v1/completions.php` in `includes/` verschieben und wiederverwenden.

### 6.2 Neue Endpunkte `[SOLL]`
Basis: `<server>/api/v1/client/` · Header `X-Client: dataKI-native/<version>` · `X-Client-Api: 1`

| Methode · Pfad | Auth | Zweck |
|---|---|---|
| `GET info` | keine | Server-Handshake: App-Version, `chat_mode`, `api_level`, Upload-Limits, erlaubte Endungen |
| `GET login/start` | keine (Browser) | Website-Login-Seite, parametriert mit `redirect_uri` (Loopback), `state`, `client_version`; zeigt E-Mail/Passwort, 2FA, Captcha, Google-Login — alles wie im normalen Web-Login, **unverändert** |
| `POST login/exchange` | `code` (Einmal-Code aus dem Redirect) | tauscht Code gegen Geräte-Key |
| `POST logout` | Bearer | Geräte-Key serverseitig widerrufen |
| `GET bootstrap` | Bearer | alles, was `chat.php` beim Laden serverseitig einbettet (2.5) + Profil + `token_status` |

Es gibt **keinen** JSON-Login-Endpunkt mit E-Mail/Passwort mehr und **keinen** separaten 2FA-Endpunkt für den Client — diese Fälle bleiben vollständig auf der Website (`login_handler.php`, `login_2fa.php` `[IST]`, unverändert). Der native Client kennt nur den Browser-Handoff.

**`GET login/start`** — öffnet im System-Browser (nicht im Client selbst):
```
GET https://ai.datanet.ovh/api/v1/client/login/start
    ?redirect_uri=http://127.0.0.1:<port>/callback
    &state=<zufällig, vom Client erzeugt>
    &device_name=Desktop+%C2%B7+mein-pc
    &client_version=0.1.0
```
- Setzt intern eine eigene Session auf, zeigt den bestehenden Login-Screen (inkl. Captcha/2FA/Google-Login), merkt sich `redirect_uri`+`state`+`device_name` für die Dauer der Session.
- Nach erfolgreichem `auth_attempt_login()`/`auth_complete_2fa()` **statt** JSON-Antwort ein `302 Location: <redirect_uri>?code=<einmaliger Code>&state=<state>` (nur bei exaktem `state`-Match).
- Einmal-Code: zufällig, server-seitig an `user_id`+`device_name` gebunden, TTL ~2 Minuten, nach einmaligem Einlösen sofort ungültig.

**`POST login/exchange`**
```http
POST /api/v1/client/login/exchange
Content-Type: application/json

{ "code": "…", "device_name": "Desktop · mein-pc", "client_version": "0.1.0" }
```
```json
{ "status": "ok", "device_key": "dk_…", "user": { "id": 1, "name": "…", "email": "…", "role": "user" } }
{ "status": "invalid_or_expired", "error": "…" }
```

**`GET bootstrap`** (Beispielstruktur)
```json
{
  "user": { "id": 1, "name": "…", "email": "…", "role": "user" },
  "chat_mode": "proxy",
  "token_status": { "tier": "free", "limit": 1000000, "tier_used": 12345, "purchased": 50000, "blocked": false },
  "models": [ { "id": "…", "label": "…", "model": "…", "tier": "free", "thinking": true, "vision": false, "effort": true } ],
  "effort_levels": [ { "id": "hoch", "label": "Hoch", "temperature": 0.7, "thinking": true, "pro": false } ],
  "effort_default": "mittel",
  "features": { "websearch": true, "research": true, "knowledge": true, "tools": true, "image": true, "voice": true, "code": true },
  "file_actions": [ … ],
  "limits": { "upload_max_bytes": 20971520, "allowed_ext": ["pdf","png", "…"] }
}
```
Quelle der Felder: `get_chat_models()`, `get_effort_levels()`, `get_effort_default()`, `get_file_actions()`, `get_plus_menu()`, `get_chat_ui_config()`, `proxy_me_get()` (nur `proxy`-Modus) `[IST]`.

### 6.3 Vorab zu behebende Auffälligkeiten (betreffen den Client direkt)
| # | Datei | Problem | Auswirkung auf den Client |
|---|---|---|---|
| 1 | `includes/proxy_client.php` → `proxy_chat_stream()` | Nur HTTP 403 wird als Fehler behandelt. Bei 401/429/5xx mit reinem JSON-Body (ohne `data:`) wird der Body ignoriert. | Client bekäme einen **leeren Assistant-Turn** ohne Fehlermeldung. → HTTP-Status prüfen und als `error`-Event ausgeben. |
| 2 | `api/v1/completions.php` | `$sseBuffer` wird nach **jedem** Chunk geleert; über Chunk-Grenzen geteilte JSON-Zeilen gehen verloren. `chat_handler.php` puffert korrekt bis zum Zeilenende. | Betrifft nur das OpenAI-Gateway (nicht den Pfad A), aber gleiche Logik angleichen. |
| 3 | `file_list_handler.php` (delete, `source=db`) | prüft `is_file($row['file_path'])`, gespeichert ist aber `/uploads_storage/…` → physische Datei wird vermutlich nicht gelöscht. `chat_handler.php` löst es korrekt über `UPLOAD_DIR` + `basename`. | Datei-Löschen aus dem Client räumt den Speicher nicht auf. |
| 4 | `config.php` | `display_errors=1` | PHP-Warnungen können den JSON-/SSE-Body verunreinigen → im Produktivbetrieb aus. |

> **Empfohlen:** die Sende-Logik aus `handle_send_message()` in eine Funktion (z. B. `chat_send_stream($user, $params, $emit)`) auslagern, damit Web-UI, Client und Gateway **denselben** Code nutzen.

---

## 7. API-Vertrag für die GUI

Alle Aufrufe: `Authorization: Bearer <Geräte-Key>` (außer `info`, `login/start`, `login/exchange`). Pfade relativ zur festen Server-URL `ai.datanet.ovh` (kann serverseitig trotzdem einen **Unterordner** enthalten, z. B. `https://ai.datanet.ovh/server/` — Basis-URL nie hart auf Domain-Root setzen, vgl. `url()`-Helper `[IST]`).

### 7.1 Chat senden `POST chat_handler.php` (multipart/form-data) `[IST]`
| Feld | Typ | Bedeutung |
|---|---|---|
| `action` | string | `send_message` |
| `content` | string | Nachrichtentext (darf leer sein) |
| `chat_id` | int | 0/leer = neuer Chat |
| `model` | string | Modell-ID aus `bootstrap.models[].id` |
| `project_id` | int | optional, nur bei neuem Chat |
| `effort` | string | Aufwandsstufen-ID |
| `thinking` | `0`/`1` | Denken an/aus |
| `websearch`, `research`, `knowledge` | `0`/`1` | Features |
| `collection_id` | int | bei `knowledge=1` |
| `files[]` | Datei | Anhänge (max. `limits.upload_max_bytes`) |

### 7.2 Antwort: SSE-Stream, Zeilen `data: {json}\n\n` `[IST]`
| `type` | Felder | GUI-Verhalten |
|---|---|---|
| `chat_id` | `chat_id` | neuen Chat in Liste eintragen |
| `thinking_delta` | `delta` | in einklappbarem „Denken"-Bereich anhängen |
| `thinking_done` | — | Denk-Bereich als fertig markieren |
| `content_delta` | `delta` | an Antworttext anhängen (nur `type`, kein `choices`) |
| `tool_status` | `id`, `tool`, `state`, `label`, `detail` | Statuszeile je `id` aktualisieren |
| `sources` | `sources[]` = `{file, upload_id, snippet, score}` | Quellenliste unter der Antwort |
| `attachment` | `attachment` = `{id,name,mime,size,url,kind,text}` | erzeugte Datei anzeigen/herunterladen |
| `error` | `code`, `message`, `upgrade`, `blocked_until` | Fehlerblase; bei `upgrade`/`MODEL_NOT_ALLOWED` Pro-Hinweis |
| `done` | `message_id`, `token_status`, `upload_ids` | Stream beenden, Token-Footer aktualisieren |

Hinweise:
- Im `proxy`-Modus kommen Kontingent-/Tier-Sperren (⛔/🔒) als **normaler `content_delta`-Text** (HTTP 200) — nicht als Fehler behandeln.
- **Zwei Fehlerarten unterscheiden:** Vor dem Streaming antwortet der Handler mit normalem JSON und HTTP-Status (z. B. 401 „Nicht angemeldet", 403 „nicht verifiziert", `{"success":false,"error":…}`); ab Streaming-Beginn kommen Fehler als `error`-Event im Stream. Der Client prüft daher zuerst `Content-Type`/HTTP-Status und parst jede SSE-Zeile defensiv (unbekannte `type`-Werte ignorieren).

### 7.3 Weitere Aufrufe (Übersicht) `[IST]`
| Funktion | Aufruf | Phase |
|---|---|---|
| Chats auflisten | `GET chat_handler.php?action=list_chats` | 1 |
| Verlauf | `GET …?action=get_messages&chat_id=` | 1 |
| Suche | `GET …?action=search_chats&q=` | 2 |
| Export | `GET …?action=export_chat&chat_id=&format=md\|json` | 2 |
| Löschen | `POST action=delete_chat` (`chat_id`, `with_files=0\|1`) | 1 |
| Nachricht bearbeiten / verzweigen | `POST action=edit_message` / `branch_chat` (`message_id`) | 2 |
| Feedback | `POST feedback_handler.php` | 3 |
| Projekte | `project_handler.php` (list, create, rename, instructions, delete, assign) | 2 |
| Wissensbasen | `collection_handler.php` (list, files, create, rename, delete, add_file, remove_file) | 3 |
| Tools | `tool_handler.php` (list, toggle) | 3 |
| Dateien | `file_list_handler.php` (list, list_children, delete) · `file_actions_handler.php` | 3 |
| Deep Research | `research_handler.php` start → Polling `status&job_id=` alle ~3 s | 3 |
| Gedächtnis | `memory_handler.php` (list, delete) | 3 |
| Bild | `image_handler.php` (`prompt`) → `{url}` oder `{b64}` | 4 |
| STT/TTS | `voice_handler.php` (`stt`: `audio` base64 + `mime`; `tts`: `text`) | 4 |
| Code ausführen | `code_handler.php` (`language`, `code`) → `stdout/stderr/images` | 4 |

Konvention: Antworten `{ "success": bool, … }`; bei nicht verfügbaren Proxy-Funktionen `{ "success": false, "reason": "unavailable" }` → Funktion in der GUI **ausblenden bzw. Hinweis** (graceful degradation wie in der WebUI).

---

## 8. GUI-Spezifikation

### 8.1 Fenster und Bereiche
```
┌──────────────────────────────────────────────────────────────────────────┐
│ Headerbar:  [☰] dataKI        Modell ▾   Aufwand ▾        Token-Stand  ⚙ │
├───────────────┬──────────────────────────────────────────────────────────┤
│ Sidebar       │  ChatView (scrollbar)                                    │
│  [+ Neuer     │   ┌ Nutzer ───────────────────────────────────────┐      │
│     Chat]     │   └───────────────────────────────────────────────┘      │
│  Suche 🔎     │   ┌ Assistent ────────────────────────────────────┐      │
│  Projekte ▸   │   │ ▸ Denken (einklappbar)                        │      │
│  Chats        │   │ Markdown · Codeblöcke [Kopieren]              │      │
│   · …         │   │ Quellen · Werkzeug-Status                     │      │
│   · …         │   └───────────────────────────────────────────────┘      │
│               ├──────────────────────────────────────────────────────────┤
│               │ Composer: [📎] [ Eingabefeld (mehrzeilig) ]  [Senden/⏹]  │
│               │           [Websuche] [Wissensbasis] [Denken] …           │
└───────────────┴──────────────────────────────────────────────────────────┘
```

### 8.2 Verhalten (MVP)
- **Senden:** `Enter` sendet, `Shift+Enter` Zeilenumbruch. Während des Streams wird „Senden" zu „⏹ Abbrechen".
- **Abbrechen:** Verbindung kappen (Server bricht via `ignore_user_abort(false)` ab `[IST]`); bis dahin empfangener Text bleibt stehen.
- **Auto-Scroll** nur, solange der Nutzer nahe am unteren Rand ist (wie `scrollToBottom()` der WebUI).
- **Denken-Bereich:** standardmäßig eingeklappt nach Abschluss.
- **Modellwahl:** aus `bootstrap.models`; Pro-Modelle bei Tier `free` mit Schloss markieren (Server entscheidet endgültig). Letzte Wahl wird lokal gemerkt.
- **Token-Footer** nur wenn `chat_mode=proxy`: `tier`, `tier_used / limit`, `purchased`, `blocked`. Bei Sperre Hinweis + Button „Im Browser öffnen".
- **Chat-Titel:** vom Server (erste 60 Zeichen der ersten Nachricht).
- **Sprache:** i18n per gettext, zuerst `de` + `en` (die WebUI hat 10 Sprachen; `lang/*.php` dient als Textvorlage).
- **Fehler:** nie modale Dialogflut — Fehler als Blase im Chat oder Banner; 401 → siehe Zustandsautomat.

### 8.3 Einstellungen
Abmelden (widerruft Key) · Standard-Modell/-Aufwand · Schriftgröße · Theme (System/Hell/Dunkel) · Netzwerk (Proxy-Einstellungen des Systems nutzen) · „Geräte im Browser verwalten". (Kein „Server-URL ändern" in v1 — Server-URL ist fest, siehe 1.4/15 #10.)

---

## 9. Client-Interna (C/GTK3, Linux + Windows)

### 9.1 Abhängigkeiten
| Zweck | Linux | Windows (MSYS2/mingw-w64) |
|---|---|---|
| UI | `gtk+-3.0` | `mingw-w64-x86_64-gtk3` |
| Code-Hervorhebung | `gtksourceview-3.0` | `mingw-w64-x86_64-gtksourceview3` |
| HTTP/SSE | `libcurl` (Write-Callback für Streaming) | `mingw-w64-x86_64-curl` |
| JSON | `json-glib-1.0` | `mingw-w64-x86_64-json-glib` |
| Keyring | `libsecret-1` | Windows Credential Manager (`Advapi32`, keine Zusatz-Lib nötig) |
| Markdown | `md4c` | `mingw-w64-x86_64-md4c` |
| Config | GLib `GKeyFile` (XDG) | GLib `GKeyFile` (`%APPDATA%\dataki\client.ini`) |
| i18n | gettext | `mingw-w64-x86_64-gettext` |
| Build | Meson + Ninja | Meson + Ninja (MSYS2-Shell oder Cross-Compile von Linux) |
| Installer | `.deb` (dpkg-deb/fpm), AppImage (`linuxdeploy` + `appimagetool`) | NSIS (`makensis`, Sprachdatei Deutsch) |
| (optional, später) | `webkit2gtk-4.x` (Canvas), GStreamer (Mikrofon/TTS) | entfällt (kein WebKit2GTK unter Windows) |

### 9.2 Projektstruktur
```
dataki-client/
├─ meson.build
├─ data/            .desktop, Icons, GResource (CSS, UI-Dateien), AppStream-Metainfo
├─ packaging/
│  ├─ linux/        dpkg-Kontrolldatei/fpm-Optionen, AppImage-Recipe (linuxdeploy)
│  └─ windows/       installer.nsi (NSIS, Sprache Deutsch), .ico, Dateiliste
├─ po/              Übersetzungen (u. a. de.po, zieht auch den NSIS-Installer-Text, 14)
├─ src/
│  ├─ main.c · app.c                 GtkApplication, Lebenszyklus
│  ├─ core/        (kein GTK-Include!)
│  │   ├─ config.c        Config (server_url, last_user, UI-Optionen; XDG unter Linux, %APPDATA% unter Windows)
│  │   ├─ secret.c        gemeinsame SecretStore-Schnittstelle (store/lookup/clear)
│  │   ├─ secret_linux.c  Implementierung über libsecret
│  │   ├─ secret_win.c    Implementierung über Windows Credential Manager (Advapi32)
│  │   ├─ http.c          curl-Wrapper, Worker-Threads, Cancel
│  │   ├─ sse.c           Zeilen-/Event-Parser (Zustand über Chunk-Grenzen)
│  │   ├─ api_auth.c      info/login-start (Browser+Loopback)/login-exchange/logout/bootstrap
│  │   ├─ api_chat.c      send/list/get/delete/search/…
│  │   ├─ api_misc.c      projects/collections/tools/files/…
│  │   ├─ auth_state.c    Zustandsautomat (Abschnitt 5.2)
│  │   └─ model.c         Structs: Chat, Message, Model, TokenStatus, Source, …
│  └─ ui/
│      ├─ login_dialog.c  main_window.c  sidebar.c  chat_view.c
│      ├─ message_widget.c  md_render.c  composer.c  settings.c
├─ build.sh         baut Linux- und Windows-Artefakte (14.1)
└─ tests/           sse_test.c, auth_state_test.c, mock_server.py
```
`secret.c` wählt zur Build-Zeit (`meson.build`, `host_machine.system()`) zwischen `secret_linux.c` und `secret_win.c`; beide implementieren dieselben Funktionssignaturen, sodass `core/` ansonsten plattformunabhängig bleibt.

### 9.3 Threading-Regeln
- **GTK nur im Main-Thread.** Netzwerk in **Worker-Threads** (`GThread`/`GTask`); Ergebnisse per `g_idle_add()` / `g_main_context_invoke()` zurück in die UI.
- Ein Request = ein curl-Easy-Handle = ein Thread. **Abbruch** über ein atomares Flag, das im curl-Progress-Callback (`CURLOPT_XFERINFOFUNCTION`) geprüft wird.
- Timeouts: `CONNECTTIMEOUT=10s`; für den Stream **kein** Gesamt-Timeout, stattdessen `LOW_SPEED_LIMIT=1` / `LOW_SPEED_TIME=120s` (der Server hat 300 s Limit zum Proxy `[IST]`).
- UI-Updates beim Streaming **bündeln** (z. B. `g_timeout_add(40 ms)`), nicht pro Token neu layouten.

### 9.4 SSE-Parser (kritisch)
- Eingehende Bytes in einen **Puffer** schreiben; nur bis zum **letzten `\n`** verarbeiten, Rest behalten (JSON-Zeilen kommen über Chunk-Grenzen geteilt an — das ist genau der Fehler aus 6.3 Nr. 2).
- Zeilen mit `data:` auswerten, `[DONE]` und leere Zeilen ignorieren, ungültiges JSON **überspringen** (nicht abbrechen), unbekannte `type`-Werte ignorieren.
- Parser ist **GTK-frei** und mit Unit-Tests abgedeckt (zerhackte Chunks, UTF-8-Zeichen über Chunk-Grenze, sehr lange Zeilen, CRLF).

### 9.5 Speicher/Robustheit
- Ownership-Regeln konsequent (`g_autoptr`/`g_autofree`), keine Rohzeiger über Thread-Grenzen ohne Referenzzählung.
- Maximale Antwortgröße/Nachrichtenzahl pro Ansicht begrenzen, ältere Nachrichten bei Bedarf lazy laden.
- CI mit **ASan/UBSan** und `valgrind`-Lauf auf den Tests.

### 9.6 Markdown-Rendering (siehe ADR-3)
- Blocktypen: Absatz, Überschrift, Liste, Zitat, Codeblock (Sprache aus Fence), Tabelle, Trennlinie.
- Links nur `https:`/`http:` öffnen (`gtk_show_uri_on_window`), kein Auto-Laden von **Remote-Bildern** (Privatsphäre) → Platzhalter + Link.
- Codeblock: Kopieren-Button; Sprache → `GtkSourceLanguage`; später „Im Canvas öffnen"/„Ausführen" (Phase 4).
- Formeln (LaTeX) sind v1 **nicht** Teil des Umfangs.

---

## 10. Sicherheit

1. **TLS-Prüfung immer an** (`CURLOPT_SSL_VERIFYPEER=1`, `VERIFYHOST=2`). `http://` nur für `localhost`/`127.0.0.1` ohne Warnung, sonst deutlicher Hinweis und Bestätigung.
2. **Keine Geheimnisse in Logs**, Crash-Dumps oder Fehlermeldungen (Key, Passwort, Authorization-Header maskieren).
3. **Passwort kommt im Client nie vor** — es wird ausschließlich auf der Website im Browser eingegeben (5.1); der Client sieht nur den Einmal-Code und tauscht ihn gegen den Geräte-Key.
4. Geräte-Key nur im plattform-eigenen Secret-Store (ADR-4); serverseitig nur als Hash; widerrufbar; Logout ruft `POST logout` auf.
5. **Keine Telemetrie** im Client. (Die WebUI enthält einen deklarierten täglichen Herkunfts-Ping `[IST: includes/phone_home.php]` — der gehört zum Server, nicht zum Client; Urheber-/Copyright-Hinweis im „Über"-Dialog ausweisen.)
6. Server-seitig: Rate-Limit auf `login/start`, `login/exchange`; Konten-Sperrlogik greift wie in der Web-Variante (Achtung: Sperre nach 6 Fehlversuchen kann zur Konto-DoS missbraucht werden → IP-Rate-Limit zusätzlich).
7. Dateiupload: Größe/Endungen **clientseitig vorprüfen** (Werte aus `bootstrap.limits`), Server prüft endgültig.
8. Antworten des Servers sind **nicht vertrauenswürdig**: Längen begrenzen, JSON defensiv parsen, nie Antworttext als Format-String verwenden.

---

## 11. Lokale Daten und Konfiguration

| Was | Linux | Windows |
|---|---|---|
| Config | `$XDG_CONFIG_HOME/dataki/client.ini` | `%APPDATA%\dataki\client.ini` |
| Inhalt Config | `server_url` (fest, für später vorbereitet), `last_user`, `last_model`, `last_effort`, Theme, Fenstergeometrie | dito |
| Geräte-Key | Secret Service, Schema `ovh.datanet.dataki`, Attribute `server`, `user` | Windows Credential Manager, Zielname `dataKI:ai.datanet.ovh` |
| Cache (optional) | `$XDG_CACHE_HOME/dataki/` | `%LOCALAPPDATA%\dataki\cache\` |
| Logs | `$XDG_STATE_HOME/dataki/client.log` | `%LOCALAPPDATA%\dataki\client.log` |

Inhalt in beiden Fällen: Cache = heruntergeladene Anhänge/Bilder (löschbar); Logs = nur Technik, ohne Inhalte/Keys.

Keine Chat-Inhalte lokal persistent speichern (außer bewusst exportierte Dateien).

---

## 12. Phasenplan und Akzeptanzkriterien

Aufwand grob: **S** ≈ Tage · **M** ≈ 1–2 Wochen · **L** > 2 Wochen (ein Entwickler).

### Phase 0 — Backend-Vorarbeit (PHP) · M
- Auth-Erweiterung (6.1), `info`/`login`/`login/2fa`/`logout`/`bootstrap` (6.2), Bearer-Durchreichung im Root-`.htaccess`.
- Auffälligkeiten 6.3 Nr. 1, 3, 4 behoben.
- **Abnahme:** per `curl` ist ein kompletter Ablauf möglich: `info` → `login/start` (Browser, liefert Redirect-Code) → `login/exchange` → `bootstrap` → `chat_handler.php send_message` mit Bearer → SSE kommt an → `logout` → danach 401. Admin-Seite mit Geräte-Key → abgelehnt.

### Phase 1 — MVP · L
- Projekt-Skeleton (Meson), `SecretStore`, `HttpClient`, `SseParser` (+Tests), `AuthManager`.
- „Anmelden"-Button mit Browser-Handoff (Loopback-Redirect, Zustandsautomat 5.2/5.5), automatischer Start mit gespeichertem Key.
- Hauptfenster: Chat-Liste, Verlauf, Senden mit Streaming, Abbrechen, Modellwahl, Token-Footer, Plain-Text-Rendering.
- **Abnahme:** Neuinstallation → „Anmelden" → Browser-Login → Chat funktioniert; App neu starten → **ohne** erneute Eingabe direkt im Chat; Key im Dashboard widerrufen → App fordert beim nächsten Request Login an; Netz weg → Offline-Banner, kein Datenverlust, kein Key-Verlust; läuft gegen `proxy`- **und** `direct`-Instanz; `build.sh` erzeugt auf einem Linux-CI-Host alle vier Artefakte (`.deb`, AppImage, Windows-`.exe`, NSIS-Installer); der NSIS-Installer läuft auf Deutsch durch und installiert eine startfähige Windows-App.

### Phase 2 — Komfort · M
- Markdown/Codeblöcke (ADR-3), Denken-Bereich, Aufwandsstufen, Suche, Export, Bearbeiten/Verzweigen, Projekte, Chat umbenennen/löschen (inkl. „Dateien mitlöschen?"-Frage).
- **Abnahme:** Antworten mit Tabellen, Listen, Code werden korrekt gerendert, auch **während** des Streamings flüssig (kein Ruckeln bei langen Antworten).

### Phase 3 — Dateien & Zusatzfunktionen · L
- Anhänge (Dialog + Drag&Drop, Limits), Datei-Ansicht inkl. Archiv-Ordner, Datei-Aktionen.
- Wissensbasen (RAG) + Quellenanzeige, Tools an/aus, Gedächtnis, Feedback, Deep Research mit Fortschritt.
- **Abnahme:** PDF/ZIP anhängen und befragen; RAG-Quellen sichtbar; Tool-Status-Events erscheinen.

### Phase 4 — Erweiterungen · L
- Bildgenerierung, STT/TTS, Code-Ausführung, Canvas-Vorschau (WebKit), Google-Login (siehe 15), Tray/Benachrichtigungen, Flatpak/Paketierung.

---

## 13. Test und Qualität

- **Unit-Tests (GTK-frei):** SSE-Parser, Auth-Zustandsautomat, JSON-Mapping, URL-Bildung (Unterordner!).
- **Mock-Server** (`tests/mock_server.py`): liefert `info`/`login`/`bootstrap` und einen SSE-Stream mit absichtlich zerhackten Chunks, Fehlerfällen (401/429/5xx, abgebrochene Verbindung, ungültiges JSON).
- **Integrationstests** gegen eine echte WebUI-Instanz in **beiden** Modi (`proxy`, `direct`) und mit/ohne 2FA.
- **Checkliste manuell:** Unterordner-Installation, Self-signed-Zertifikat (soll scheitern), langsamer Server, sehr lange Antworten, Emoji/CJK über Chunk-Grenzen, 10-MB-Anhang, Server-Neustart während des Streams.
- **CI:** Build + Tests + ASan/UBSan, `clang-tidy`/`cppcheck`.

---

## 14. Build und Auslieferung

### 14.1 `build.sh` — ein Skript für beide Plattformen `[SOLL]`
Ein Skript im Projekt-Root baut **beide** Zielplattformen und liefert fertige, installierbare Artefakte ab:

```bash
./build.sh            # baut alles, was auf dem aktuellen Host möglich ist (siehe unten)
./build.sh linux       # nur Linux-Artefakte
./build.sh windows     # nur Windows-Artefakte (Cross-Compile, falls auf Linux ausgeführt)
./build.sh clean       # entfernt build/ und dist/
```

**Ablauf (Linux-Host, Standardfall):**
1. Linux-Build: `meson setup build-linux && meson compile -C build-linux`.
2. `.deb` paketieren (Kontrolldatei aus `packaging/linux/`, `dpkg-deb --build` bzw. `fpm`).
3. AppImage bauen: Binary + Libs mit `linuxdeploy` bündeln, `appimagetool` erzeugt `dataki-client-<version>-x86_64.AppImage`.
4. Windows-Cross-Build: `meson setup build-windows --cross-file packaging/windows/mingw-cross.ini && meson compile -C build-windows` (Toolchain `x86_64-w64-mingw32-gcc`, Libs aus MSYS2/mingw-w64, siehe 9.1).
5. Windows-DLLs der Abhängigkeiten neben die `.exe` kopieren (`ldd`/`ntldd` zum Ermitteln der nötigen DLLs).
6. NSIS-Installer bauen: `makensis packaging/windows/installer.nsi` → `dataki-client-<version>-setup.exe`.
7. Alle Artefakte landen in `dist/`:
   - `dataki-client_<version>_amd64.deb`
   - `dataki-client-<version>-x86_64.AppImage`
   - `dataki-client-<version>-setup.exe` (Windows-Installer)
   - optional `dataki-client-<version>-windows-x64.zip` (portable Variante ohne Installer, für Debug/CI)

Wird `build.sh` direkt **unter Windows** (MSYS2-Shell) ausgeführt, entfällt der Cross-Compile-Schritt (5./6. laufen nativ), die Linux-Schritte (1.–3.) werden dann übersprungen bzw. müssen separat auf einem Linux-Host laufen (kein Linux-Cross-Build von Windows aus vorgesehen).

### 14.2 NSIS-Installer — auf Deutsch `[SOLL]`
- `packaging/windows/installer.nsi` nutzt **MUI2**; Sprache **Deutsch** als einzige bzw. Standard-Sprache:
  ```nsis
  !include "MUI2.nsh"
  !insertmacro MUI_LANGUAGE "German"
  ```
- Installer-Texte (Willkommen, Lizenz, Installationsort, Fertig) entsprechend auf Deutsch; Lizenztext = Copyright-Hinweis `dataNet.ovh Ltd.` (siehe 10.5/15 #9).
- Installer legt an: Programmdateien unter `%ProgramFiles%\dataKI Client`, Startmenü-Verknüpfung, optional Desktop-Verknüpfung (Checkbox), `.exe`-Dateizuordnung nicht nötig.
- Deinstallation über Standard-`Uninstall.exe`, entfernt auch ggf. abgelegte Config unter `%APPDATA%\dataki` nur auf Nachfrage (Chat-Verlauf liegt ohnehin nur auf dem Server, siehe 1.4/11).
- Versionsinfo (Datei-Version, Produktname, Copyright) im `.exe`-Ressourcenblock (`windres`) hinterlegen.

### 14.3 Linux-Pakete
- Meson-Build, Zielsystem zunächst aktuelle Debian/Ubuntu/Fedora.
- `.deb` und **AppImage** als primäre Linux-Artefakte (14.1); **Flatpak** als spätere Ergänzung prüfen (Keyring-Zugriff dann über Secret-Service-Portal).
- `.desktop`-Datei, Icon, AppStream-Metainfo, gettext-Kataloge.

### 14.4 Gemeinsames
- Versionierung: `client_version` im Header; `api_level` aus `GET info` — bei zu altem Client Hinweis „Update erforderlich" (gilt für beide Plattformen).
- CI baut beide Plattformen bei jedem Tag/Release und lädt alle vier Artefakte aus 14.1 Punkt 7 hoch.

---

## 15. Offene Entscheidungen (bitte klären)

| # | Frage | Empfehlung |
|---|---|---|
| 1 | Pfad A (Bearer in bestehenden Handlern) oder B (neue REST-Schicht)? | **A**, B später schrittweise — **entschieden** |
| 2 | Login im Client: nativ (E-Mail/Passwort/2FA/Captcha) oder Browser-Handoff? | **entschieden: immer Browser-Handoff** (5.1) — kein natives Login/2FA/Captcha-UI im Client, auch für v1/MVP, nicht erst Phase 4 |
| 3 | `direct`-Modus ohne Login-Zwang (Gast-Konten, bisher session-gebunden): Client-Zugang zulassen? | v1: Login (Browser-Handoff) verlangen; Gastzugang später über `POST client/guest` |
| 4 | Google-Login im Client? | **entfällt als Einzelfrage:** Google-Login ist Teil der Website-Login-Seite und damit ab v1 automatisch über den Browser-Handoff abgedeckt (kein separater Phase-4-Mechanismus nötig) |
| 5 | Keyring nicht verfügbar: Datei-Fallback (`0600`) oder Abbruch? | Fallback mit sichtbarer Warnung, abschaltbar |
| 6 | Markdown: md4c-Widgets (nativ) oder WebKit-Nachrichtenliste? | md4c-Widgets; WebKit nur Canvas |
| 7 | Plattformen: nur Linux oder auch Windows/macOS? | **entschieden: Linux + Windows in v1** (GTK3 via MSYS2/mingw-w64, NSIS-Installer Deutsch, 14); macOS weiterhin nicht v1 |
| 8 | Gültigkeit der Geräte-Keys (unbegrenzt vs. 90 Tage + Auto-Verlängerung)? | unbegrenzt, jederzeit widerrufbar; `expires_at` vorbereiten |
| 9 | Name/Branding/Lizenz des Clients (Copyright-Vermerk `dataNet.ovh Ltd.`)? | wie WebUI; Hinweis im „Über"-Dialog |
| 10 | Server-URL konfigurierbar? | **entschieden: v1 fest `ai.datanet.ovh`**; „eigenen Server hinzufügen" als Soll-Feature später (1.3/1.4) |

---

## 16. Anhang

### 16.1 Manueller Test per curl (Ziel-Verhalten nach Phase 0)
```bash
BASE=https://ai.datanet.ovh

curl -s "$BASE/api/v1/client/info"

# login/start im Browser öffnen (manuell), Website-Login durchführen,
# Redirect liefert ?code=… an http://127.0.0.1:<port>/callback

curl -s -X POST "$BASE/api/v1/client/login/exchange" \
  -H 'Content-Type: application/json' \
  -d '{"code":"…","device_name":"curl-test","client_version":"0.0.0"}'
# → {"status":"ok","device_key":"dk_…", …}

KEY=dk_…
curl -s "$BASE/api/v1/client/bootstrap" -H "Authorization: Bearer $KEY"

curl -N -s -X POST "$BASE/chat_handler.php" \
  -H "Authorization: Bearer $KEY" \
  -F action=send_message -F model=llama3 -F content="Hallo" -F chat_id=0
# → data: {"type":"chat_id",…} / content_delta … / done
```

### 16.2 Mapping Web-UI-Funktion → Client-Phase
| WebUI | Client |
|---|---|
| Login/2FA/Captcha/Google-Login | Phase 1 — **immer Browser-Handoff** (5.1), kein natives UI im Client |
| Registrierung, Passwort vergessen, Verifizierung | Browser-Handoff |
| Chat, Streaming, Denken, Aufwand | Phase 1–2 |
| Suche, Export, Bearbeiten, Verzweigen | Phase 2 |
| Projekte | Phase 2 |
| Dateien, Archive, RAG, Tools, Research, Gedächtnis | Phase 3 |
| Bild, Sprache, Code, Canvas | Phase 4 |
| Dashboard (Profil, 2FA, Rechnungen), Kauf | Browser-Handoff |
| Admin-Panel | Browser |

### 16.3 Glossar
- **Browser-Handoff / Loopback-Redirect:** Login-Muster, bei dem der Client einen lokalen Port öffnet, den System-Browser auf die Website schickt und nach erfolgreichem Login per Redirect einen Einmal-Code auf diesem Port zurückbekommt (analog OAuth-Loopback-Flow); ab v1 der **einzige** Login-Weg des Clients (5.1).
- **Proxy / ollama-proxy:** externer Dienst, Quelle für Tier/Tokens/Pakete (nur im `proxy`-Modus).
- **Geräte-Key (`dk_…`):** neuer, pro Installation erzeugter Key des Native-Clients.
- **`server_key`:** bisheriger, vom Nutzer erzeugter Key für das OpenAI-Gateway.
- **`proxy_api_key`:** Nutzer-Key der WebUI gegenüber dem Proxy — nie im Client.
- **SSE:** Server-Sent Events, `data: {json}` pro Zeile.
