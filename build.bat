@echo off
rem ============================================================================
rem build.bat — 一键构建 mew_combat（C++ 战斗核心 + 单元测试）
rem 用法:  build.bat           Release 构建
rem         build.bat debug     Debug 构建
rem 自动定位 VS2022（vswhere）与其自带 CMake/Ninja，无需手动配置环境变量。
rem 之后阶段会扩展为：C++ -> 测试 -> GDExtension -> godot/bin 全流程。
rem ============================================================================
setlocal EnableExtensions
cd /d "%~dp0"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] vswhere not found. Please install Visual Studio 2022 with the C++ workload.
    exit /b 1
)
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo [ERROR] Visual Studio 2022 with C++ tools not found.
    exit /b 1
)
echo [INFO] Visual Studio: %VSDIR%

set "CMAKE=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJA=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if not exist "%CMAKE%" (
    echo [ERROR] CMake not found in VS installation.
    exit /b 1
)

set "BUILD_TYPE=Release"
if /i "%~1"=="debug" set "BUILD_TYPE=Debug"

call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] vcvars64.bat failed.
    exit /b 1
)

rem MSVC diagnostics in English (easier to share/fix); console output as UTF-8
set VSLANG=1033
chcp 65001 >nul

echo [STEP 1/3] Configure ^(codegen + Ninja^)...
"%CMAKE%" -S cpp -B cpp\build -G Ninja -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_MAKE_PROGRAM="%NINJA%"
if errorlevel 1 goto :fail

echo [STEP 2/3] Build...
"%CMAKE%" --build cpp\build
if errorlevel 1 goto :fail

echo [STEP 3/4] Unit tests...
"%CMAKE%" --build cpp\build --target test
if errorlevel 1 goto :fail

echo [STEP 4/5] Deploy GDExtension DLL to godot\bin...
if not exist godot\bin mkdir godot\bin
copy /Y cpp\build\gdext-build\mew_battle.dll godot\bin\ >nul
if errorlevel 1 goto :fail

echo [STEP 5/5] Prepare Godot assets (wiki-asset -^> godot\assets)...
python tools\prepare_godot_assets.py
if errorlevel 1 goto :fail

echo [OK] build completed (%BUILD_TYPE%).
exit /b 0

:fail
echo [ERROR] build failed.
exit /b 1
