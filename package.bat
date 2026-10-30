@echo off
rem Builds the Fluffy Mod Manager zip (also fine for a manual install) from
rem build\hid.dll and the files in package\fluffy. Settings are not shipped:
rem the mod keeps them in Documents\Playable Ellie (WORKING).
rem
rem   package.bat          release\PlayableEllieWorking-v<version>.zip
rem   package.bat test     release\test\PlayableEllieWorking-v<version>-test.zip
rem
rem Run build.bat first. Never overwrites a zip that already exists.
setlocal
set VERSION=1.0.0
cd /d "%~dp0"
if not exist build\hid.dll (
  echo build\hid.dll not found. Run build.bat first.
  exit /b 1
)
set OUT=release
set ZIP=release\PlayableEllieWorking-v%VERSION%.zip
if /i "%~1"=="test" (
  set OUT=release\test
  set ZIP=release\test\PlayableEllieWorking-v%VERSION%-test.zip
)
if exist "%ZIP%" (
  echo %ZIP% already exists; delete it first if you really want to replace it.
  exit /b 1
)
copy /y build\hid.dll package\fluffy\ >nul || exit /b 1
rem Empty log so Fluffy records it and removes it on uninstall.
type nul > package\fluffy\PlayableEllie.log
if not exist %OUT% mkdir %OUT%
powershell -NoProfile -Command "Compress-Archive -Path package\fluffy\modinfo.ini,package\fluffy\screenshot.png,package\fluffy\hid.dll,package\fluffy\PlayableEllie.log,package\fluffy\PlayableEllie-README.txt -DestinationPath '%ZIP%'" || exit /b 1
echo Built %ZIP%
