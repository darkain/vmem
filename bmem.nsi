; example1.nsi
;
; This script is perhaps one of the simplest NSIs you can make. All of the
; optional settings are left to their default settings. The installer simply 
; prompts the user asking them where to install, and drops a copy of makensisw.exe
; there. 

;--------------------------------

; The name of the installer
Name "bmem"

; The file to write
OutFile "bmem-install.exe"

; The default installation directory
InstallDir $PROGRAMFILES\bmem

InstallDirRegKey HKLM "Software\bmem" "Install_Dir"

AutoCloseWindow true

ComponentText "Please select from the following installation options." "" "Options:"

;--------------------------------

; Pages

Page components
Page directory
Page instfiles

;--------------------------------

; The stuff to install
Section "-bmem" ;No components page, name is not important

  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  
  ; Put file there
  File Release\bmem.exe
  File license.txt
  File whatsnew.txt

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\bmem" "DisplayName" "bmem"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\bmem" "UninstallString" '"$INSTDIR\bmem-uninst.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\bmem" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\bmem" "NoRepair" 1
  
  WriteUninstaller "bmem-uninst.exe"
SectionEnd ; end the section

Section "Make Start Menu entries"
  CreateDirectory "$SMPROGRAMS\bmem"
  CreateShortcut "$SMPROGRAMS\bmem\bmem.lnk" "$INSTDIR\bmem.exe"
  CreateShortcut "$SMPROGRAMS\bmem\Uninstall bmem.lnk" "$INSTDIR\bmem-uninst.exe" "Uninstall bmem"
SectionEnd ; end the section

Section "Make bmem run at startup"
  CreateShortcut "$SMPROGRAMS\Startup\bmem.lnk" "$INSTDIR\bmem.exe"
;  WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Run" "bmem" "$INSTDIR\bmem.exe" 
SectionEnd ; end the section

Function .onInstSuccess
  MessageBox MB_YESNO "Start BMEM now?" IDNO Neinen
    Exec "$INSTDIR\bmem.exe"
  Neinen:
FunctionEnd

Section "Install source code"
  SetOutPath $INSTDIR\source
  File bmem.h
  File bmem.cpp
  File bmem.dsp
  File appbar.*
  File config.h
  File cpu.*
  File cpucount.h
  File cpucount.cpp
  File cpucount.dsp
  File diskio.*
  File viewport.*
  File ipdh.h
  File wa.h
  File wa.cpp
  File resource.h
  File icon1.ico
  File bmem.rc
  File bmem.nsi
  File libctiny.lib
SectionEnd ; end the section

Section "Uninstall"
  MessageBox MB_OKCANCEL "Are you sure you want to uninstall BMEM?" IDOK killit IDCANCEL nevermind

killit:
  Delete "$SMPROGRAMS\Startup\bmem.lnk"
  Delete "$INSTDIR\bmem.exe"
  Delete "$INSTDIR\license.txt"
  Delete "$INSTDIR\whatsnew.txt"
  Delete "$INSTDIR\bmem-uninst.exe"
  Delete "$INSTDIR\source\*.*"
  RMDir "$INSTDIR\source"
  RMDir "$INSTDIR"

  Delete "$SMPROGRAMS\bmem\*.lnk"
  RMDir "$SMPROGRAMS\bmem"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\bmem"
  DeleteRegKey HKLM SOFTWARE\bmem

;  DeleteRegKey HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Run" "bmem" "$INSTDIR\bmem.exe" 

nevermind:
SectionEnd ; end the section
