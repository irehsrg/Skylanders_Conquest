@echo off
echo Generating Visual Studio project files...
echo.

rem Find Unreal Engine installation
set UE_PATH=C:\Program Files\Epic Games\UE_5.6

rem Check if path exists
if not exist "%UE_PATH%\Engine\Build\BatchFiles\Build.bat" (
    echo ERROR: Unreal Engine not found at: %UE_PATH%
    echo Please edit this file and update UE_PATH to your UE5.6 installation
    pause
    exit /b 1
)

rem Generate project files
"%UE_PATH%\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="%~dp0Skylanders_Conquest.uproject" -game -engine

echo.
echo Done! You can now open Skylanders_Conquest.sln in Visual Studio
pause
