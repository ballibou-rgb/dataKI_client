; ---------------------------------------------------------------------------
; dataKI Client — NSIS installer (MUI2)
;
; Language selection dialog on startup (German default + English).
; Build:  makensis -DVERSION=0.1.0 -DSRCDIR=<staging-dir> installer.nsi
;   SRCDIR must contain dataki-client.exe plus all runtime DLLs and the
;   share/ tree (icons, glib schemas, gtksourceview, locale) — prepared by
;   build.sh / CI.
; ---------------------------------------------------------------------------

Unicode true

!ifndef VERSION
  !define VERSION "0.1.0"
!endif
!ifndef SRCDIR
  !define SRCDIR "staging"
!endif

!define APPNAME       "dataKI Client"
!define COMPANY       "dataNet.ovh Ltd."
!define EXENAME       "dataki-client.exe"
!define REGUNINST     "Software\Microsoft\Windows\CurrentVersion\Uninstall\dataKI Client"
!define INSTALLSIZE   120000   ; KB estimate for Add/Remove Programs

Name "${APPNAME}"
OutFile "dataki-client-${VERSION}-setup.exe"
InstallDir "$PROGRAMFILES64\dataKI Client"
InstallDirRegKey HKLM "Software\dataKI Client" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName"     "${APPNAME}"
VIAddVersionKey "CompanyName"     "${COMPANY}"
VIAddVersionKey "LegalCopyright"  "© ${COMPANY}"
VIAddVersionKey "FileDescription" "${APPNAME} Setup"
VIAddVersionKey "FileVersion"     "${VERSION}"
VIAddVersionKey "ProductVersion"  "${VERSION}"

; ---------------------------------------------------------------------------
; MUI2
; ---------------------------------------------------------------------------
!include "MUI2.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON   "dataki.ico"
!define MUI_UNICON "dataki.ico"

; Remember the language selection
!define MUI_LANGDLL_REGISTRY_ROOT      "HKLM"
!define MUI_LANGDLL_REGISTRY_KEY       "Software\dataKI Client"
!define MUI_LANGDLL_REGISTRY_VALUENAME "InstallerLanguage"

; Offer to launch on the finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\${EXENAME}"
!define MUI_FINISHPAGE_RUN_TEXT "$(RunAppText)"

; ---------------------------------------------------------------------------
; Pages
; ---------------------------------------------------------------------------
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE $(LicenseData)
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Languages — German first so it is the default highlighted entry
!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "English"

; ---------------------------------------------------------------------------
; Localised strings (defined after the languages exist)
; ---------------------------------------------------------------------------
; Per-language license text
LicenseLangString LicenseData ${LANG_GERMAN}  "license_de.txt"
LicenseLangString LicenseData ${LANG_ENGLISH} "license_en.txt"

LangString SecMainName    ${LANG_GERMAN}  "Programmdateien (erforderlich)"
LangString SecMainName    ${LANG_ENGLISH} "Program files (required)"
LangString SecDesktopName ${LANG_GERMAN}  "Desktop-Verknüpfung erstellen"
LangString SecDesktopName ${LANG_ENGLISH} "Create desktop shortcut"
LangString SecStartName   ${LANG_GERMAN}  "Startmenü-Verknüpfung erstellen"
LangString SecStartName   ${LANG_ENGLISH} "Create Start Menu shortcut"
LangString RunAppText     ${LANG_GERMAN}  "dataKI jetzt starten"
LangString RunAppText     ${LANG_ENGLISH} "Launch dataKI now"
LangString UninstConfirm  ${LANG_GERMAN}  "Möchten Sie auch Ihre Einstellungen unter %APPDATA%\dataki entfernen?"
LangString UninstConfirm  ${LANG_ENGLISH} "Do you also want to remove your settings under %APPDATA%\dataki?"

; ---------------------------------------------------------------------------
; Install sections
; ---------------------------------------------------------------------------
Section "$(SecMainName)" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "${SRCDIR}\*.*"

  WriteRegStr HKLM "Software\dataKI Client" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\dataKI Client" "Version" "${VERSION}"

  ; Add/Remove Programs entry
  WriteRegStr   HKLM "${REGUNINST}" "DisplayName"     "${APPNAME}"
  WriteRegStr   HKLM "${REGUNINST}" "DisplayVersion"  "${VERSION}"
  WriteRegStr   HKLM "${REGUNINST}" "Publisher"       "${COMPANY}"
  WriteRegStr   HKLM "${REGUNINST}" "DisplayIcon"     "$INSTDIR\${EXENAME}"
  WriteRegStr   HKLM "${REGUNINST}" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr   HKLM "${REGUNINST}" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKLM "${REGUNINST}" "NoModify" 1
  WriteRegDWORD HKLM "${REGUNINST}" "NoRepair" 1
  WriteRegDWORD HKLM "${REGUNINST}" "EstimatedSize" ${INSTALLSIZE}

  WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd

Section "$(SecStartName)" SecStart
  CreateDirectory "$SMPROGRAMS\dataKI"
  CreateShortcut  "$SMPROGRAMS\dataKI\dataKI.lnk" "$INSTDIR\${EXENAME}" "" "$INSTDIR\${EXENAME}" 0
  CreateShortcut  "$SMPROGRAMS\dataKI\dataKI deinstallieren.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section /o "$(SecDesktopName)" SecDesktop
  CreateShortcut "$DESKTOP\dataKI.lnk" "$INSTDIR\${EXENAME}" "" "$INSTDIR\${EXENAME}" 0
SectionEnd

; ---------------------------------------------------------------------------
; Uninstaller
; ---------------------------------------------------------------------------
Section "Uninstall"
  Delete "$DESKTOP\dataKI.lnk"
  Delete "$SMPROGRAMS\dataKI\dataKI.lnk"
  Delete "$SMPROGRAMS\dataKI\dataKI deinstallieren.lnk"
  RMDir  "$SMPROGRAMS\dataKI"

  RMDir /r "$INSTDIR"

  DeleteRegKey HKLM "${REGUNINST}"
  DeleteRegKey HKLM "Software\dataKI Client"

  ; Config removal only on request (chat history lives on the server anyway)
  MessageBox MB_YESNO|MB_ICONQUESTION "$(UninstConfirm)" IDNO skip_config
    RMDir /r "$APPDATA\dataki"
  skip_config:
SectionEnd

; ---------------------------------------------------------------------------
; Init — show the language selection dialog
; ---------------------------------------------------------------------------
Function .onInit
  !insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd

Function un.onInit
  !insertmacro MUI_UNGETLANGUAGE
FunctionEnd
