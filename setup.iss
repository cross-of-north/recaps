#define AppVersion GetFileVersion('x64\Release\Recaps.exe')

[Setup]
AppName=Recaps Switcher
AppVerName=Recaps Switcher
AppVersion={#AppVersion}
AppPublisher=Siaržuk Žarski
DefaultDirName={autopf}\Recaps
DisableDirPage=yes
DefaultGroupName=Recaps
DisableProgramGroupPage=yes
SetupIconFile=res\recaps.ico
InfoBeforeFile=readme.txt
Compression=lzma
SolidCompression=yes
;AppMutex=recaps-D3E743A3-E0F9-47f5-956A-CD15C6548789
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=commandline dialog
ArchitecturesInstallIn64BitMode=x64
OutputDir=DistOutput
OutputBaseFilename=ReCaps_V{#AppVersion}

[Files]
Source: "x64\Release\recaps.exe"; DestDir: "{app}"; Flags: ignoreversion; Check:Is64BitInstallMode
Source: "Win32\Release\recaps.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: "readme.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "flags\*.ico"; DestDir: "{app}\flags"; Flags: ignoreversion

[InstallDelete]
Type: files; Name: "{app}\flags\flag_*.ico"

[Icons]
Name: "{group}\Recaps Switcher"; Filename: "{app}\recaps.exe"; WorkingDir: "{app}"
Name: "{group}\Unistall"; Filename: "{uninstallexe}"

[Registry]
Root: HKCU; Subkey: "Software\Recaps"; Flags: uninsdeletekey

[Run]
Filename: "{app}\recaps.exe"; Description: "{cm:LaunchProgram,Recaps}"; Flags: nowait postinstall skipifsilent

[Code]
const
  WM_CLOSE = 16;

function CloseRecapsApp : Boolean;
var winHwnd: Longint;
    strProg: string;
begin
  Result := True;
  try
    strProg := 'RECAPS';
    winHwnd := FindWindowByClassName(strProg);
    if winHwnd <> 0 then
      Result := PostMessage(winHwnd,WM_CLOSE,0,0);
  except
  end;
end;

function InitializeSetup : Boolean;
begin
	Result := CloseRecapsApp();
end;

function InitializeUninstall : Boolean;
begin
	Result := CloseRecapsApp();
end;

