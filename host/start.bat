@echo off
:: Snake Robot Upper Computer - Windows Quick Start
:: Requires Node.js 16+ : https://nodejs.org

where node >nul 2>nul
if %errorlevel% neq 0 (
  echo [ERROR] Node.js not found. Please install Node.js 16+ from https://nodejs.org
  pause
  exit /b 1
)

cd /d "%~dp0"

if not exist node_modules (
  echo Installing dependencies...
  call npm install
  if %errorlevel% neq 0 (
    echo [ERROR] npm install failed. Check your network and Node.js version.
    pause
    exit /b 1
  )
)

echo Starting backend server...
node server.js
pause
