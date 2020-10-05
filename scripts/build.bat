@echo off

setlocal

set BAT_DIR=%~dp0

pushd %BAT_DIR%\..

set OutputDirs=-Febin\main.exe -Fobuild\ -Fdbuild\
set CommonCompilerFlags=-Zi -FC -GR- -EHa- -nologo
set CommonLinkerFlags=-PDB:build\main.pdb -INCREMENTAL:NO -MACHINE:X64 user32.lib

set VulkanDir=C:\VulkanSDK\1.2.148.1
set CommonCompilerFlags=%CommonCompilerFlags% -I%VulkanDir%\Include
set CommonLinkerFlags=%CommonLinkerFlags% -LIBPATH:%VulkanDir%\Lib vulkan-1.lib

REM Optimization switches /O2
REM set CommonCompilerFlags=-MTd -nologo -fp:fast -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -FC -Z7

IF NOT EXIST build mkdir build
IF NOT EXIST bin   mkdir bin

REM Executable
cl %OutputDirs% %CommonCompilerFlags% code\main.cpp /link %CommonLinkerFlags%

REM Shaders
glslc code\vertex_shader.glsl   -o bin\vertex_shader.spv
glslc code\fragment_shader.glsl -o bin\fragment_shader.spv

popd
