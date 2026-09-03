#ifndef MyVersion
  #define MyVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\\release\\RelWithDebInfo"
#endif

[Setup]
AppId={{6CBAD6F0-11DD-48B5-992D-D7085B788293}
AppName=Fortnite Map Rotation for OBS
AppVersion={#MyVersion}
AppPublisher=ぱんるく
DefaultDirName={commonappdata}\obs-studio\plugins\fortnite-map-rotation
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=fortnite-map-rotation-{#MyVersion}-windows-x64-Installer
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Fortnite Map Rotation for OBS
CloseApplications=yes

[Languages]
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"

[Files]
Source: "{#SourceDir}\fortnite-map-rotation\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
function InitializeSetup(): Boolean;
begin
  if CheckForMutexes('OBSStudio') then
  begin
    MsgBox('OBS Studioを終了してから、もう一度インストーラーを実行してください。', mbError, MB_OK);
    Result := False;
  end
  else
    Result := True;
end;
