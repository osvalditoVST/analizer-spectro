@echo off
net session >nul 2>&1
if %errorlevel% neq 0 (
  echo Ejecuta este archivo como ADMINISTRADOR ^(clic derecho - Ejecutar como administrador^).
  pause & exit /b 1
)
set DEST=%CommonProgramFiles%\VST3
xcopy /E /I /Y "%~dp0Genre Spectrum.vst3" "%DEST%\Genre Spectrum.vst3"
echo.
echo Instalado en %DEST%. Abre tu DAW y reescanea los plugins.
pause
