@echo off
cd %~dp0
xmrig.exe --bench=1M --submit --algo=gr
pause
