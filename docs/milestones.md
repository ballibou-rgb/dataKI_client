# dataKI Client — Meilensteine & Entscheidungen

Ergänzung zum Architekturdokument (`architecture.md`, Entwurf 0.3). Hält die im
Projekt getroffenen Entscheidungen und die Umsetzungsreihenfolge fest.

## Getroffene Entscheidungen

| Thema | Entscheidung |
|---|---|
| Plattformen | Linux + Windows (C/GTK3) **und** Android (Java) |
| Repo | Monorepo: `desktop/`, `android/`, `docs/`, `.github/` |
| Startversion | `0.1.0` |
| Linux-Ziel | min. Debian 12 (bookworm) / Ubuntu 24.04 |
| M1-Linking (Desktop) | nur GTK3; übrige Libs (curl, json-glib, md4c, secret) ab M2 |
| NSIS-Texte | direkt als `LangString` im `.nsi` (nicht aus po) |
| Windows-Installer | NSIS MUI2, **Sprachauswahl beim Start** (Deutsch/Englisch) |
| Menü-Stil (Desktop) | moderne Headerbar mit ☰-Menü (keine klassische Menüleiste) |
| Fenster schließen | X → in Tray verstecken; Beenden nur via ☰→Beenden oder Tray→Beenden |
| Tray Linux | `libayatana-appindicator3` (Fallback: X beendet, wenn kein Tray) |
| Tray Windows | Win32 `Shell_NotifyIcon` |
| Android applicationId | `ovh.datanet.dataki.client` |
| Android minSdk | 26 (Android 8), targetSdk 34 |
| Android UI | Java + klassische Views/XML + Material Components |
| Android Login-Rücksprung | **App-Links (https)** → braucht serverseitig `assetlinks.json` |
| CI Windows-Build | MSYS2 nativ (robuster als Cross-Compile) |

## Roadmap

### M1 — Gerüst + Paketierung  ✅ (dieser Stand)
Lauffähige Hüllen aller drei Clients, Tray-Verhalten, i18n DE/EN, komplette
Paketierung (`.deb`, AppImage, NSIS DE/EN, ZIP) und Release-Pipeline. **Kein**
echter Login/Chat.

### M2 — Backend-Phase (PHP)  ✅
Umgesetzt und end-to-end getestet (MariaDB 10.11 + PHP 8.3). Liegt als
Drop-in-Paket unter `backend/` (neue `/api/v1/client/*`-Endpunkte,
`includes/client_auth.php`, Auto-Migration `server_keys` + `client_login_codes`,
Bearer-Geräte-Key in `auth_current_user()`, Root-`.htaccess`-Passthrough,
zwei kleine Patches). Bestehende Handler (z. B. `chat_handler.php`) akzeptieren
den Geräte-Key unverändert (Option A). Siehe `backend/README.md`.
Offen: App-Links `assetlinks.json` (braucht Signatur-Fingerprint).

### M3 — Login (alle Clients)
Desktop: Browser-Handoff per `127.0.0.1`-Loopback. Android: Custom-Tab →
App-Link-Rücksprung. Geräte-Key im Secret-Store, Auth-Zustandsautomat, Auto-Start,
`bootstrap`.

### M4 — Chat (alle Clients)
SSE-Streaming, Modellwahl, Denken-Anzeige, Abbrechen, Chat-Liste/Verlauf,
Token-Footer. Desktop: md4c-Rendering, Android: Markwon.

### M5+ — Komfort & Zusatzfunktionen
Suche, Export, Projekte, Dateien, RAG, Tools, Research, Bild/Sprache/Code
(Architektur-Phasen 2–4).

## Branding

Das vom Nutzer gelieferte Logo ist eingebaut: die Bildmarke (Gehirn/Klammern)
wurde freigestellt (Text + Hintergrund entfernt, transparent) und in alle
Icon-Formate überführt — Windows `.ico` (Multi-Größe), Linux hicolor-PNGs
(16–512), Android Adaptive Icon (Vordergrund freigestellt, weißer Hintergrund).
Quell-/Markendateien liegen unter `brand/`.

## Offene Punkte (vom Nutzer zu liefern)

- **Webprojekt** (PHP) für M2.
- **App-Signatur-Fingerprint** (SHA-256) aus dem Android-Keystore → für
  `assetlinks.json` (M2/M3).
