@echo off
setlocal EnableExtensions EnableDelayedExpansion
title Luma3DS v9.1 / v13.4 build

rem ==================================================================
rem  Luma3DS dual-version build launcher (Windows)
rem
rem  Runs the existing build.sh inside devkitPro's MSYS2 bash,
rem  because Makefiles here need Unix tools (make, sed, awk, git,
rem  armips, makerom, firmtool) that cmd.exe cannot run directly.
rem
rem  One-time setup:
rem    1) Install devkitPro with 3DS support
rem         https://devkitpro.org/wiki/Getting_Started
rem         dkp-pacman -S 3ds-dev
rem       NOTE: the 3ds-dev group is 3dslink / 3dstools / devkitARM /
rem       devkitARM-gdb / general-tools / picasso / tex3ds.  armips is NOT
rem       in it and must be installed separately (v9.1 needs it; v13.4
rem       does not).  Get armips from
rem         https://github.com/Kingcom/armips/releases
rem       and drop armips.exe into a folder on PATH, e.g.
rem         C:\devkitPro\msys2\usr\local\bin
rem       The 32-bit build also needs the Visual C++ 2015 x86 Redistributable,
rem       otherwise it fails to launch and make reports error 127.
rem    2) pip install firmtool
rem    3) Install 7-Zip (used to auto-extract the inner .7z sources)
rem
rem  Usage:
rem    build.bat                          build both
rem    set ONLY=v9.1  ^& build.bat         build v9.1 only
rem    set ONLY=v13.4 ^& build.bat         build v13.4 only
rem    set KEEP_BUILD=1 ^& build.bat       skip make clean (incremental)
rem    set STRICT=1 ^& build.bat           turn all warnings into errors
rem    set V134_CTRU=v2.6.2 ^& build.bat   override libctru for v13.4
rem  (chain multiple "set" lines before build.bat if needed)
rem ==================================================================

cd /d "%~dp0"

echo.
echo === Locating devkitPro (MSYS2) ===
set "BASH="
if defined DEVKITPRO if exist "%DEVKITPRO%\msys2\usr\bin\bash.exe" set "BASH=%DEVKITPRO%\msys2\usr\bin\bash.exe"
if not defined BASH if exist "C:\devkitPro\msys2\usr\bin\bash.exe" set "BASH=C:\devkitPro\msys2\usr\bin\bash.exe"
if not defined BASH if exist "%SystemDrive%\devkitPro\msys2\usr\bin\bash.exe" set "BASH=%SystemDrive%\devkitPro\msys2\usr\bin\bash.exe"

if not defined BASH (
  echo.
  echo [ERROR] Could not find devkitPro's MSYS2 bash.exe
  echo   1^) Install devkitPro: https://devkitpro.org/wiki/Getting_Started
  echo   2^) If installed somewhere other than C:\devkitPro, set it first, e.g.:
  echo        set DEVKITPRO=D:\devkitPro
  echo.
  pause
  exit /b 1
)
echo   bash: "%BASH%"

echo.
echo === Extracting inner source archives (.7z) if needed ===
call :extract Luma3DS_v9_1  Luma3DS_v9_1.7z
call :extract Luma3DS_v13_4 Luma3DS_v13_4.7z

rem verify each tree actually has a Makefile before building
set "MISSING=0"
if not exist "Luma3DS_v9_1\Makefile"  ( echo   [ERROR] Luma3DS_v9_1\Makefile not found.  & set "MISSING=1" )
if not exist "Luma3DS_v13_4\Makefile" ( echo   [ERROR] Luma3DS_v13_4\Makefile not found. & set "MISSING=1" )
if "%MISSING%"=="1" (
  echo.
  echo   The source trees are not extracted correctly.
  echo   Extract Luma3DS_v9_1.7z into a folder named  Luma3DS_v9_1
  echo   and Luma3DS_v13_4.7z into a folder named      Luma3DS_v13_4
  echo   ^(each archive's Makefile must sit directly inside that folder^),
  echo   then run build.bat again.
  echo.
  pause
  exit /b 1
)

rem CHERE_INVOKING=1 stops the MSYS2 login shell from cd-ing to $HOME,
rem so build.sh runs from this folder. build.sh locates itself anyway.
set "CHERE_INVOKING=1"
set "MSYSTEM=MSYS"

echo.
echo === Building (running build.sh in MSYS2) ===
echo.
"%BASH%" -l build.sh
set "RC=%ERRORLEVEL%"

echo.
if "%RC%"=="0" (
  echo ==========================================================
  echo  BUILD OK. Copy the contents of the  dist  folder to the
  echo  root of your SD card ^(overwrite the existing boot.firm^).
  echo ==========================================================
) else (
  echo ==========================================================
  echo  BUILD FAILED ^(code %RC%^). See build.log for details.
  echo  To retry only v13.4:   set ONLY=v13.4 ^& build.bat
  echo ==========================================================
)
echo.
pause
exit /b %RC%

rem ---- extract <archive> into folder <dir> (skip if <dir> exists) ----
:extract
set "DEST=%~1"
set "ARCH=%~2"
if not exist "%ARCH%" (
  echo   [WARN] %ARCH% not found.
  goto :eof
)
rem Re-extract when the archive has changed since the folder was made.
rem Skipping purely because the folder exists silently builds stale sources.
set "STAMP=%DEST%\.extracted-from"
for %%F in ("%ARCH%") do set "ASTAMP=%%~tF %%~zF"
set "OLDSTAMP="
if exist "%STAMP%" set /p OLDSTAMP=<"%STAMP%"
if exist "%DEST%\" if "!OLDSTAMP!"=="!ASTAMP!" (
  echo   %DEST%\ matches %ARCH%, skipping extraction.
  goto :eof
)
if exist "%DEST%\" echo   %ARCH% differs from %DEST%\ - refreshing sources.
set "SEVENZIP="
for %%P in (7z.exe) do if not defined SEVENZIP if not "%%~$PATH:P"=="" set "SEVENZIP=%%~$PATH:P"
if not defined SEVENZIP if exist "%ProgramFiles%\7-Zip\7z.exe" set "SEVENZIP=%ProgramFiles%\7-Zip\7z.exe"
if not defined SEVENZIP if exist "%ProgramFiles(x86)%\7-Zip\7z.exe" set "SEVENZIP=%ProgramFiles(x86)%\7-Zip\7z.exe"
if not defined SEVENZIP (
  echo   [NOTE] 7-Zip not found. Please extract %ARCH% manually into a
  echo          folder named  %DEST%  and run build.bat again.
  goto :eof
)
"%SEVENZIP%" x -y "-o%DEST%" "%ARCH%" >nul
if errorlevel 1 (
  echo   [WARN] Failed to extract %ARCH%.
) else (
  > "%STAMP%" echo !ASTAMP!
  echo   %ARCH% -^> %DEST%\
)
goto :eof
