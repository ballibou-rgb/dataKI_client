#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# dataKI desktop client — build & packaging orchestrator.
#
#   ./build.sh            build everything possible on this host
#   ./build.sh linux      Linux artifacts (.deb, AppImage)
#   ./build.sh windows    Windows artifacts (cross-compile + NSIS setup + zip)
#   ./build.sh clean      remove build/ and dist/
#
# On a Linux host the Windows build is a mingw-w64 cross-compile (needs the
# toolchain + mingw GTK3 stack). In CI we build Windows natively under MSYS2,
# which is more reliable — see .github/workflows/release.yml.
# ---------------------------------------------------------------------------
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"

DIST="$HERE/dist"
VERSION="$(sed -n "s/^[[:space:]]*version:[[:space:]]*'\\([^']*\\)'.*/\\1/p" meson.build | head -n1)"
: "${VERSION:=0.1.0}"

log()  { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[warn]\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31m[error]\033[0m %s\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------------------
clean() {
  log "Cleaning build/ and dist/"
  rm -rf "$HERE/build-linux" "$HERE/build-windows" "$DIST"
}

# ---------------------------------------------------------------------------
build_linux() {
  command -v meson >/dev/null || die "meson not found"
  command -v ninja >/dev/null || die "ninja not found"

  log "Linux: configuring & compiling (v$VERSION)"
  meson setup build-linux --prefix=/usr --buildtype=release --wipe >/dev/null 2>&1 \
    || meson setup build-linux --prefix=/usr --buildtype=release
  meson compile -C build-linux

  log "Linux: running tests"
  meson test -C build-linux || warn "tests reported failures"

  local appdir="$HERE/build-linux/appdir"
  rm -rf "$appdir"
  DESTDIR="$appdir" meson install -C build-linux >/dev/null
  mkdir -p "$DIST"

  package_deb "$appdir"
  package_appimage "$appdir"
}

package_deb() {
  local appdir="$1"
  command -v dpkg-deb >/dev/null || { warn "dpkg-deb not found — skipping .deb"; return; }

  log "Linux: building .deb"
  local debroot="$HERE/build-linux/debroot"
  rm -rf "$debroot"
  mkdir -p "$debroot"
  cp -a "$appdir/usr" "$debroot/usr"
  mkdir -p "$debroot/DEBIAN"
  sed "s/@VERSION@/$VERSION/g" packaging/linux/control.in > "$debroot/DEBIAN/control"

  local deb="$DIST/dataki-client_${VERSION}_amd64.deb"
  dpkg-deb --build --root-owner-group "$debroot" "$deb"
  log "Linux: wrote $(basename "$deb")"
}

package_appimage() {
  local appdir="$1"
  local ld ai
  ld="$(command -v linuxdeploy || true)"
  ai="$(command -v appimagetool || true)"
  if [ -z "$ld" ] || [ -z "$ai" ]; then
    warn "linuxdeploy/appimagetool not found — skipping AppImage (CI provides them)."
    return
  fi

  log "Linux: building AppImage"
  local out="$DIST/dataki-client-${VERSION}-x86_64.AppImage"
  ARCH=x86_64 OUTPUT="$out" "$ld" \
    --appdir "$appdir" \
    --plugin gtk \
    --desktop-file "$appdir/usr/share/applications/ovh.datanet.dataki.client.desktop" \
    --icon-file "$appdir/usr/share/icons/hicolor/scalable/apps/ovh.datanet.dataki.client.svg" \
    --output appimage
  log "Linux: wrote $(basename "$out")"
}

# ---------------------------------------------------------------------------
build_windows() {
  if [ "$(uname -s)" != "Linux" ]; then
    build_windows_native
    return
  fi

  command -v x86_64-w64-mingw32-gcc >/dev/null \
    || { warn "mingw-w64 toolchain not found — skipping Windows cross-build. Build natively under MSYS2 or in CI."; return; }

  log "Windows: cross-compiling (v$VERSION)"
  meson setup build-windows --cross-file packaging/windows/mingw-cross.ini \
    --prefix=/ --buildtype=release --wipe >/dev/null 2>&1 \
    || meson setup build-windows --cross-file packaging/windows/mingw-cross.ini --prefix=/ --buildtype=release
  meson compile -C build-windows

  stage_windows "$HERE/build-windows/dataki-client.exe"
}

build_windows_native() {
  command -v meson >/dev/null || die "meson not found (run inside the MSYS2 mingw64 shell)"
  log "Windows (native MSYS2): compiling (v$VERSION)"
  meson setup build-windows --buildtype=release --wipe >/dev/null 2>&1 \
    || meson setup build-windows --buildtype=release
  meson compile -C build-windows
  stage_windows "$HERE/build-windows/dataki-client.exe"
}

# Assemble exe + DLLs + GTK runtime data into a staging dir, then NSIS + zip.
stage_windows() {
  local exe="$1"
  [ -f "$exe" ] || die "built exe not found: $exe"

  local staging="$HERE/build-windows/staging"
  rm -rf "$staging"; mkdir -p "$staging"
  cp "$exe" "$staging/dataki-client.exe"

  log "Windows: collecting dependent DLLs"
  collect_dlls "$exe" "$staging"
  collect_gtk_runtime "$staging"

  mkdir -p "$DIST"

  if command -v makensis >/dev/null; then
    log "Windows: building NSIS installer"
    ( cd packaging/windows && makensis -DVERSION="$VERSION" -DSRCDIR="$staging" installer.nsi )
    mv -f "packaging/windows/dataki-client-${VERSION}-setup.exe" "$DIST/"
    log "Windows: wrote dataki-client-${VERSION}-setup.exe"
  else
    warn "makensis not found — skipping installer."
  fi

  log "Windows: building portable zip"
  ( cd "$staging" && zip -qr "$DIST/dataki-client-${VERSION}-windows-x64.zip" . )
  log "Windows: wrote dataki-client-${VERSION}-windows-x64.zip"
}

# Resolve PE import DLLs, copying the non-system ones (mingw/GTK) next to the exe.
# Prefers ntldd (MSYS2), falls back to ldd (also available under MSYS2).
collect_dlls() {
  local exe="$1" dest="$2"
  if command -v ntldd >/dev/null; then
    ntldd -R "$exe" | awk '/=>/ {print $3}' \
      | grep -iE '/mingw64/|/mingw32/|/ucrt64/' \
      | sort -u | while read -r dll; do [ -f "$dll" ] && cp -n "$dll" "$dest/" || true; done
  elif command -v ldd >/dev/null; then
    ldd "$exe" | awk '/=>/ {print $3}' \
      | grep -iE '/mingw64/|/mingw32/|/ucrt64/' \
      | sort -u | while read -r dll; do [ -f "$dll" ] && cp -n "$dll" "$dest/" || true; done
  else
    warn "no ntldd/ldd — DLL collection must be done by CI"
  fi
}

# Copy GTK runtime data (icons, schemas, gtksourceview, loaders, locale).
collect_gtk_runtime() {
  local dest="$1"
  local prefix=""
  if [ -n "${MINGW_PREFIX:-}" ]; then prefix="$MINGW_PREFIX"
  elif [ -d /mingw64 ]; then prefix="/mingw64"; fi
  [ -z "$prefix" ] && { warn "GTK runtime prefix unknown — CI assembles share/ and lib/."; return; }

  mkdir -p "$dest/share/glib-2.0/schemas" "$dest/share/icons" "$dest/lib"
  cp -rn "$prefix/share/glib-2.0/schemas"/* "$dest/share/glib-2.0/schemas/" 2>/dev/null || true
  if command -v glib-compile-schemas >/dev/null; then
    glib-compile-schemas "$dest/share/glib-2.0/schemas" || true
  fi
  cp -rn "$prefix/share/icons/Adwaita"      "$dest/share/icons/" 2>/dev/null || true
  cp -rn "$prefix/share/icons/hicolor"      "$dest/share/icons/" 2>/dev/null || true
  cp -rn "$prefix/lib/gdk-pixbuf-2.0"       "$dest/lib/"         2>/dev/null || true
}

# ---------------------------------------------------------------------------
main() {
  local target="${1:-all}"
  case "$target" in
    clean)   clean ;;
    linux)   build_linux ;;
    windows) build_windows ;;
    all)     build_linux; build_windows ;;
    *)       die "unknown target '$target' (use: linux | windows | all | clean)";;
  esac
  [ -d "$DIST" ] && { log "Artifacts in dist/:"; ls -la "$DIST" 2>/dev/null || true; }
}

main "$@"
