@echo off

setlocal

set BAT_DIR=%~dp0

pushd %BAT_DIR%..\bin
devenv /debugexe main.exe
popd
