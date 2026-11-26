@echo off
setlocal
cd /d "%~dp0"
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "CORE_VS=%%i"
if not defined CORE_VS exit /b 1
call "%CORE_VS%\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
set "CORE_OUTPUT=build"
if "%ICEBORN_CORE_CROWN_BUILD%"=="1" set "CORE_OUTPUT=build-crown-dev17"
if "%ICEBORN_HUD_BUILD%"=="1" set "CORE_OUTPUT=build-hud-timers"
if not exist "%CORE_OUTPUT%" mkdir "%CORE_OUTPUT%"
pushd "%CORE_OUTPUT%" || exit /b 1
set "FLAGS=/nologo /MT /O2 /EHsc /std:c++20 /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN /I..\third_party\minhook\include"
set "GRAPHICS=..\src\overlay.cpp ..\src\dx11_surface.cpp ..\src\panel_painter.cpp ..\src\sampling.cpp ..\src\crowns.cpp"
set "LIBS=d2d1.lib dwrite.lib d3d11.lib d3d12.lib dxgi.lib d3dcompiler.lib user32.lib advapi32.lib"
cl /nologo /MT /O2 /c ..\third_party\minhook\src\buffer.c ..\third_party\minhook\src\hook.c ..\third_party\minhook\src\trampoline.c ..\third_party\minhook\src\hde\hde64.c || exit /b 1
cl %FLAGS% /LD ..\src\settings.cpp ..\src\menu_input.cpp dinput8.lib dxguid.lib ..\src\core.cpp ..\src\hooks.cpp ..\src\dxgi_bridge.cpp %GRAPHICS% buffer.obj hook.obj trampoline.obj hde64.obj %LIBS% /Fe:iceborn_suite_core.dll || exit /b 1
cl %FLAGS% /LD ..\src\adapter.cpp /Fe:iceborn_suite_adapter.dll || exit /b 1
cl %FLAGS% /DCORE_GRAPHICS_TEST ..\src\settings.cpp ..\src\menu_input.cpp dinput8.lib dxguid.lib ..\tests\render_dx11.cpp ..\src\hooks.cpp ..\src\dxgi_bridge.cpp buffer.obj hook.obj trampoline.obj hde64.obj %GRAPHICS% %LIBS% /Fe:render_dx11.exe || exit /b 1
cl %FLAGS% /LD ..\src\bootstrap.cpp /link /DEF:..\src\exports.def /OUT:xinput9_1_0.dll || exit /b 1
cl %FLAGS% ..\tests\loader_host.cpp user32.lib /Fe:loader_host.exe || exit /b 1
if not "%~1"=="" render_dx11.exe "%~1" || exit /b 1
popd
exit /b 0
