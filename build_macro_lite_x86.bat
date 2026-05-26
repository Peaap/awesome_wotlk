@echo off
setlocal
set "REPO_DIR=%~dp0"
set "BUILD_DIR=%REPO_DIR%build-codex-x86"
set "CMAKE_EXE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
if errorlevel 1 exit /b %ERRORLEVEL%

if not exist "%BUILD_DIR%\CMakeCache.txt" (
  "%CMAKE_EXE%" -S "%REPO_DIR%" -B "%BUILD_DIR%" -G "Visual Studio 18 2026" -A Win32
  if errorlevel 1 exit /b %ERRORLEVEL%
)

"%CMAKE_EXE%" --build "%BUILD_DIR%" --config Release --target AwesomeMacroLite
endlocal
