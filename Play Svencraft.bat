@echo off
rem Svencraft: launches the sandbox in our Xash3D build
start "" "%~dp0run\xash3d.exe" -game svencraft -console +sv_cheats 1 +map svencraft_sandbox
