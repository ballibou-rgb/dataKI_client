# dataKI – Build- & Deploy-Anleitung

Diese Anleitung deckt alles ab, was du brauchst, um die dataKI-Clients zu bauen
und lauffähig zu machen: **Backend (Server), Linux, Windows, Android** – inkl.
dem, was du jeweils angeben/konfigurieren musst.

> **Server-URL:** Die Clients sprechen fest mit `https://ai.datanet.ovh`.
> Zum Testen gegen einen anderen Server: Desktop über die Umgebungsvariable
> `DATAKI_SERVER_URL`, Android über `ApiConfig.BASE_URL`
> (`android/app/src/main/java/.../core/ApiConfig.java`) und
> `desktop/meson.build` (`DATAKI_SERVER_URL`).

---

## 0. Backend zuerst deployen  ⚠️ (sonst „Not Found" beim Login)

Der Login schlägt fehl (`…/api/v1/client/login/start.php` → **Not Found**),
solange die neuen Backend-Dateien **nicht auf dem Server liegen**. Die Clients
sind korrekt – es fehlt nur das Deploy. Das Paket liegt im Repo unter `backend/`.

**Auf dem Server (in deiner `dataKIWebUI_V4`-Installation):**

1. **Neue Dateien kopieren** (gleiche Pfade wie im Repo):
   ```
   includes/client_auth.php
   api/v1/client/info.php
   api/v1/client/bootstrap.php
   api/v1/client/logout.php
   api/v1/client/login/start.php
   api/v1/client/login/exchange.php
   .well-known/assetlinks.json          (nur für Android nötig, siehe §4)
   ```
2. **Root-`.htaccess` ersetzen** durch `backend/dataKIWebUI_V4/.htaccess`
   (behält deine bestehenden Schutzregeln, ergänzt die Authorization-
   Durchreichung, damit der Bearer bei `chat_handler.php` ankommt). Alternativ
   nur den `Authorization`-Block aus der Datei anhängen.
3. **Zwei Patches anwenden** (im `dataKIWebUI_V4/`-Wurzelverzeichnis):
   ```sh
   patch -p1 < backend/patches/includes__auth.php.patch
   patch -p1 < backend/patches/chat.php.patch
   ```
   (Oder die Änderungen manuell übernehmen – sie sind klein und in
   `backend/README.md` beschrieben.)
4. **Fertig.** Die DB-Migration (neue `server_keys`-Spalten +
   `client_login_codes`) läuft **automatisch** beim ersten Aufruf.

**Prüfen (ohne Client):**
```sh
curl -s https://ai.datanet.ovh/api/v1/client/info.php
# → {"success":true,"app":"dataKIWebUI","app_version":"8.0.0",...}
```
Wenn das JSON kommt, funktioniert der Desktop-Login sofort.

Details & vollständiger End-to-End-Testablauf: **`backend/README.md`**.

---

## 1. Linux-Desktop

### Voraussetzungen (Debian 12 / Ubuntu 24.04)
```sh
sudo apt-get update
sudo apt-get install -y \
  meson ninja-build build-essential pkg-config gettext \
  libgtk-3-dev libayatana-appindicator3-dev \
  libcurl4-openssl-dev libjson-glib-dev libsecret-1-dev
# für AppImage zusätzlich: linuxdeploy, linuxdeploy-plugin-gtk, appimagetool
```

### Bauen
```sh
cd desktop
./build.sh linux      # erzeugt .deb + AppImage in desktop/dist/
# oder manuell:
meson setup build && meson compile -C build && ./build/dataki-client
```

### Installieren / Starten
```sh
sudo apt install ./desktop/dist/dataki-client_0.1.0_amd64.deb
dataki-client
```
**Anzugeben:** nichts – Server ist fest. (Taskleisten-Icon erscheint nach der
Installation; uninstalliert unter Wayland zeigt der Desktop ein generisches Icon.)

---

## 2. Windows-Desktop

### Variante A – automatisch per CI (empfohlen)
Tag pushen, GitHub Actions baut und veröffentlicht alle Artefakte:
```sh
git tag v0.1.0 && git push origin v0.1.0
```
→ unter **Releases**: `dataki-client-0.1.0-setup.exe` (NSIS, Sprachauswahl
DE/EN), `…-windows-x64.zip`, plus die Linux-Artefakte.

### Variante B – lokal unter MSYS2
1. [MSYS2](https://www.msys2.org/) installieren, **MINGW64-Shell** öffnen.
2. Pakete installieren:
   ```sh
   pacman -S --needed git zip \
     mingw-w64-x86_64-gcc mingw-w64-x86_64-meson mingw-w64-x86_64-ninja \
     mingw-w64-x86_64-pkgconf mingw-w64-x86_64-gtk3 mingw-w64-x86_64-glib2 \
     mingw-w64-x86_64-gettext mingw-w64-x86_64-curl mingw-w64-x86_64-json-glib \
     mingw-w64-x86_64-nsis mingw-w64-x86_64-ntldd
   ```
3. Bauen:
   ```sh
   cd desktop
   ./build.sh windows    # → dist/dataki-client-0.1.0-setup.exe + ...-windows-x64.zip
   ```

**Anzugeben:** nichts. (Der NSIS-Installer fragt beim Start die Sprache ab,
installiert nach `%ProgramFiles%\dataKI Client`, legt Startmenü- und optional
Desktop-Verknüpfung an.)

---

## 3. Android

### Voraussetzungen
- **Android Studio** (bringt SDK + JDK mit). Minimale SDK: API 26 (Android 8).
- Internet beim ersten Sync (lädt AGP 8.5.2, Material, OkHttp, RecyclerView …).

### Öffnen & bauen
1. In Android Studio **`android/`** als Projekt öffnen → Gradle-Sync abwarten.
2. **Debug** direkt auf Gerät/Emulator per *Run* ▶.
   Oder per Kommandozeile (mit installiertem SDK):
   ```sh
   cd android
   ./gradlew assembleDebug        # APK unter app/build/outputs/apk/debug/
   ./gradlew test                 # JVM-Unit-Tests (SseParserTest)
   ```

### Release (signiertes APK/AAB)
1. In Android Studio **Build → Generate Signed Bundle / APK** → neuen **Keystore**
   anlegen (den sicher aufbewahren – er ist dauerhaft an die App gebunden).
2. AAB/APK bauen.

### App-Links (nötig, damit der Login in die App zurückspringt) ⚠️
Der Login kehrt über `https://ai.datanet.ovh/app/login/callback` in die App
zurück. Damit Android die App automatisch öffnet, muss der Server die
Verknüpfung bestätigen:

1. **Signatur-Fingerprint holen** (aus deinem Keystore):
   ```sh
   keytool -list -v -keystore <dein.keystore> -alias <alias> | grep SHA256
   ```
2. In **`.well-known/assetlinks.json`** (auf dem Server, aus `backend/`)
   `REPLACE_WITH_YOUR_APP_SIGNING_SHA256_FINGERPRINT` durch diesen Fingerprint
   ersetzen. Datei muss unter
   `https://ai.datanet.ovh/.well-known/assetlinks.json` als `application/json`
   **ohne Redirect** erreichbar sein.
3. Testest du die **Debug**-Variante, zusätzlich das Paket
   `ovh.datanet.dataki.client.debug` + dessen Debug-Fingerprint eintragen
   (Debug-Keystore: `~/.android/debug.keystore`, Passwort `android`).

**Anzugeben (Zusammenfassung Android):** Keystore (Release), dessen SHA-256-
Fingerprint → `assetlinks.json` auf dem Server.

---

## 4. Was muss wo angegeben werden? (Kurzüberblick)

| Thema | Wo | Pflicht für |
|---|---|---|
| Backend-Dateien deployen | Server `dataKIWebUI_V4/` | **Login (alle Clients)** |
| `assetlinks.json` + App-Fingerprint | Server `/.well-known/` | Android-Login |
| Android-Keystore (Release) | Android Studio | Android-Release |
| Server-URL ändern (optional) | `desktop/meson.build` + `ApiConfig.java` | nur anderer Server |
| GitHub-Token/Secrets | — | nicht nötig (Workflows nutzen `GITHUB_TOKEN`) |

Beim Chat selbst ist **nichts** zusätzlich anzugeben – er nutzt die bestehenden
Handler (`chat_handler.php`) mit dem Geräte-Key. Ob echte Antworten kommen,
hängt von deiner Server-Konfiguration ab (`CHAT_MODE` = `proxy`/`direct` und die
dort hinterlegten Modelle/Backends).

---

## 5. Release aller Desktop-Artefakte auf einmal
```sh
git tag v0.1.0
git push origin v0.1.0
```
GitHub Actions (`.github/workflows/release.yml`) baut Linux (`.deb`, AppImage)
und Windows (NSIS-Setup, ZIP) und hängt sie an ein GitHub-Release. Android baust
du in Android Studio.

---

## 6. Entwickler-Testhilfen (optional, nur Desktop)
Umgebungsvariablen – in Produktion wirkungslos:
- `DATAKI_SERVER_URL=http://127.0.0.1:8099` – gegen lokalen Server testen.
- `DATAKI_DEBUG=1` – Login-Start-URL ins Log schreiben.
- `DATAKI_TEST_SIGNIN=1` – Login automatisch starten.
- `DATAKI_TEST_KEY=dk_…` / `DATAKI_TEST_MESSAGE="…"` – Login überspringen /
  Nachricht automatisch senden (für UI-Tests, z. B. mit `tests/mock_server.py`).
