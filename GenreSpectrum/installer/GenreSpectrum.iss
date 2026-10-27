[Setup]
AppName=Genre Spectrum
AppVersion=1.0.0
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin
OutputDir=..\dist
OutputBaseFilename=GenreSpectrum_Setup_Win64
Compression=lzma
SolidCompression=yes
Uninstallable=yes

[Files]
Source: "..\dist\Genre Spectrum\Genre Spectrum.vst3\*"; DestDir: "{commoncf64}\VST3\Genre Spectrum.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
