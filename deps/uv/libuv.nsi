; NSIS installer script for libuv

!include "MUI2.nsh"

Name "libuv"
OutFile "libuv-${ARCH}-${VERSION}.exe"

!include "x64.nsh"
# Default install location, for 32-bit files
InstallDir "$PROGRAMFILES\libuv"

# Override install and registry locations if this is a 64-bit install.
function .onInit
	${If} ${ARCH} == "x64"
		SetRegView 64
		StrCpy $INSTDIR "$PROGRAMFILES64\libuv"
	${EndIf}
functionEnd

;--------------------------------
; Installer pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH


;--------------------------------
; Uninstaller pages
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

;--------------------------------
; Languages
!insertmacro MUI_LANGUAGE "English"

;--------------------------------
; Installer sections

Section "Files" SecInstall
	SectionIn RO
	SetOutPath "$INSTDIR"
	File "Release\*.dll"
	File "Release\*.lib"
	File "LICENSE"
	File "README.md"

	SetOutPath "$INSTDIR\include"
	File "include\uv.h"
	File "include\uv-errno.h"
	File "include\uv-threadpool.h"
	File "include\uv-version.h"
	File "include\uv-win.h"
	File "include\tree.h"

	WriteUninstaller "$INSTDIR\Uninstall.exe"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\libuv-${ARCH}-${VERSION}" "DisplayName" "libuv-${ARCH}-${VERSION}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\libuv-${ARCH}-${VERSION}" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\libuv-${ARCH}-${VERSION}" "QuietUninstallString" "$\"$INSTDIR\Uninstall.exe$\" /S"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\libuv-${ARCH}-${VERSION}" "HelpLink" "http://libuv.org/"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\libuv-${ARCH}-${VERSION}" "URLInfoAbout" "http://libuv.org/"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\libuv-${ARCH}-${VERSION}" "DisplayVersion" "${VERSION}"
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\libuv-${ARCH}-${VERSION}" "NoModify" "1"
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\libuv-${ARCH}-${VERSION}" "NoRepair" "1"
SectionEnd

Section "Uninstall"
	Delete "$INSTDIR\libuv.dll"
	Delete "$INSTDIR\libuv.lib"
	Delete "$INSTDIR\LICENSE"
	Delete "$INSTDIR\README.md"

	Delete "$INSTDIR\include\uv.h"
	Delete "$INSTDIR\include\uv-errno.h"
	Delete "$INSTDIR\include\uv-threadpool.h"
	Delete "$INSTDIR\include\uv-version.h"
	Delete "$INSTDIR\include\uv-win.h"
	Delete "$INSTDIR\include\tree.h"

	Delete "$INSTDIR\Uninstall.exe"
	RMDir "$INSTDIR"
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\libuv-${ARCH}-${VERSION}"
SectionEnd

