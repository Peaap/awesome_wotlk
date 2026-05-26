@echo off
setlocal

set "REPO_DIR=%~dp0"
set "BUILD_DIR=%REPO_DIR%build-codex-x86"
if "%~1"=="" (
  if "%WOW_ROOT%"=="" (
    echo Usage:
    echo   build_install_macro_lite_x86.bat C:\Path\To\Grimfall-WoW
    echo.
    echo Or set WOW_ROOT first:
    echo   set WOW_ROOT=C:\Path\To\Grimfall-WoW
    exit /b 1
  )
) else (
  set "WOW_ROOT=%~1"
)
set "CMAKE_EXE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "VCVARSALL=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"

if not exist "%VCVARSALL%" (
  echo Missing MSVC vcvarsall.bat:
  echo   %VCVARSALL%
  exit /b 1
)

if not exist "%CMAKE_EXE%" (
  echo Missing Visual Studio CMake:
  echo   %CMAKE_EXE%
  exit /b 1
)

call "%VCVARSALL%" x86
if errorlevel 1 exit /b %ERRORLEVEL%

if not exist "%BUILD_DIR%\CMakeCache.txt" (
  "%CMAKE_EXE%" -S "%REPO_DIR%." -B "%BUILD_DIR%" -G "Visual Studio 18 2026" -A Win32
  if errorlevel 1 exit /b %ERRORLEVEL%
)

"%CMAKE_EXE%" --build "%BUILD_DIR%" --config Release --target AwesomeMacroLite
if errorlevel 1 exit /b %ERRORLEVEL%

if not exist "%WOW_ROOT%" (
  echo Missing WoW root:
  echo   %WOW_ROOT%
  exit /b 1
)

copy /Y "%REPO_DIR%build\Release\AwesomeMacroLite.dll" "%WOW_ROOT%\AwesomeMacroLite.dll" >nul
if errorlevel 1 exit /b %ERRORLEVEL%

echo Installed:
echo   %WOW_ROOT%\AwesomeMacroLite.dll
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-FileHash -Algorithm SHA256 '%WOW_ROOT%\AwesomeMacroLite.dll' | Format-List"

endlocal
