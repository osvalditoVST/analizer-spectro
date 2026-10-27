@echo off
net session >nul 2>&1
if %errorlevel% neq 0 ( echo Ejecuta como ADMINISTRADOR. & pause & exit /b 1 )
rmdir /S /Q "%CommonProgramFiles%\VST3\Genre Spectrum.vst3"
echo Desinstalado.
pause
