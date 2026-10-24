#ifndef APP_VERSION
  #define APP_VERSION "0.0.0"
#endif
#ifndef SOURCE_DIR
  #define SOURCE_DIR "..\..\dist"
#endif
#ifndef OUTPUT_DIR
  #define OUTPUT_DIR "..\..\release"
#endif

[Setup]
AppId={{B7D7A3B0-3C5E-4B7A-9D3C-2F6A1E8C4F21}
AppName=Concord
AppVersion={#APP_VERSION}
AppVerName=Concord {#APP_VERSION}
AppPublisher=Concord
AppPublisherURL=https://github.com/luizhmmachado/Concord
AppSupportURL=https://github.com/luizhmmachado/Concord/issues
AppUpdatesURL=https://github.com/luizhmmachado/Concord/releases
DefaultDirName={localappdata}\Programs\Concord
DefaultGroupName=Concord
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir={#OUTPUT_DIR}
OutputBaseFilename=Concord-{#APP_VERSION}-windows-x64-setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
MinVersion=10.0
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
UninstallDisplayIcon={app}\Concord.exe
SetupLogging=yes
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SOURCE_DIR}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{userprograms}\Concord"; Filename: "{app}\Concord.exe"
Name: "{userdesktop}\Concord"; Filename: "{app}\Concord.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Concord.exe"; Description: "{cm:LaunchProgram,Concord}"; Flags: nowait postinstall skipifsilent
