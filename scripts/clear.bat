@echo off

setlocal

set BAT_DIR=%~dp0

pushd %BAT_DIR%\..
rmdir /s /q bin
rmdir /s /q build
popd
