@echo off
setlocal
set exe=calculator.exe
set src=main.c
if not exist %src% (
  echo %src% not found
  exit /b 1
)

gcc %src% -o %exe% -mwindows -O2 -Wall -Wextra
if errorlevel 1 (
  echo Build failed
  exit /b 1
)

echo Build OK: %exe%
