# dataKI Client

Native client family for **dataKI** (dataKIWebUI backend, `ai.datanet.ovh`).
Three clients that share one backend contract (`/api/v1/client/*`, Bearer device
key, SSE streaming):

| Client  | Language | Delivery                                   |
|---------|----------|--------------------------------------------|
| Linux   | C / GTK3 | `.deb`, AppImage                           |
| Windows | C / GTK3 | NSIS installer (language picker), portable ZIP |
| Android | Java     | Android Studio project (build APK/AAB yourself) |

## Repository layout

```
.
├─ desktop/     C/GTK3 client for Linux + Windows (Meson build, packaging, build.sh)
├─ android/     Android Studio project (Java, Gradle, applicationId ovh.datanet.dataki.client)
├─ docs/        Architecture document + milestone notes
└─ .github/     CI (build check) and release (tag v* → GitHub Release) workflows
```

## Status — Milestone 1 (scaffold + packaging)

This milestone delivers runnable *shells* of all three clients plus the full
packaging and release pipeline. **Login and chat are not implemented yet** —
they follow in later milestones once the backend `/api/v1/client/*` endpoints
exist (see `docs/architecture.md`, sections 5–7).

What works in M1:

- **Desktop:** GTK3 window with header bar (☰ menu: *About*, *Quit*),
  placeholder sidebar / chat view / composer, **tray behaviour** (closing the
  window hides to tray; quit only via ☰ menu or tray menu), German/English UI.
- **Android:** openable Android Studio project, launchable Material shell,
  `core/` skeleton, App-Links prepared, encrypted key storage stub.
- **Packaging:** `desktop/build.sh` produces `.deb`, AppImage, NSIS setup
  (DE/EN picker) and a portable ZIP.
- **Release:** push a tag `v0.1.0` → GitHub Actions builds and publishes the
  desktop artifacts to a GitHub Release.

## Building

### Desktop (Linux)

```sh
cd desktop
./build.sh linux        # .deb + AppImage  (or: meson setup build && meson compile -C build)
```

### Desktop (Windows)

Cross-compile from Linux (`./build.sh windows`) or build natively in an MSYS2
mingw-w64 shell. See `desktop/packaging/windows/`.

### Android

Open the `android/` folder in Android Studio and build/run as usual.

## License

Copyright © dataNet.ovh Ltd. See `desktop/packaging/windows/license_*.txt`.
