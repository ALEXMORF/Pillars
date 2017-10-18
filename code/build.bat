@echo off

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

cl -FC -Fepillars.exe -nologo -Z7 -W4 -wd4127 -wd4100 -wd4201 -wd4005 -wd4505 -wd4189 ..\code\main.cpp user32.lib winmm.lib gdi32.lib opengl32.lib /link -incremental:no -subsystem:windows

popd

