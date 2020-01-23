@echo off

REM Optimization switches /O2
REM set CommonCompilerFlags=-MTd -nologo -fp:fast -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -FC -Z7 -I..\libraries\glfw\include  -I..\libraries\glm\include -IC:\VulkanSDK\1.1.130.0\Include
REM set OutputDirs=-Fobuild\main.out 
REM-Fdbuild\main.pdb -Febuild\main.exe
set OutputDirs=-Febuild\ -Fobuild\ -Fdbuild\
set CommonCompilerFlags=-EHsc -Zi -Ilibraries\glfw\include  -Ilibraries\glm\include -IC:\VulkanSDK\1.1.130.0\Include
set CommonLinkerFlags=-LIBPATH:libraries\glfw\lib-vc2017 -LIBPATH:C:\VulkanSDK\1.1.130.0\Lib glfw3dll.lib vulkan-1.lib

IF NOT EXIST build mkdir build
REM pushd build

REM 32-bit build
REM cl -Fobuild %CommonCompilerFlags% main.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags%

REM 64-bit build
cl %OutputDirs% %CommonCompilerFlags% main.cpp /link %CommonLinkerFlags%

REM popd
