#!/usr/bin/env bash
# Snake Robot Upper Computer - Linux/macOS Quick Start
# Requires Node.js 16+ : https://nodejs.org

set -e
cd "$(dirname "$0")"

if ! command -v node >/dev/null 2>&1; then
  echo "[ERROR] Node.js not found. Please install Node.js 16+ from https://nodejs.org"
  exit 1
fi

if [ ! -d node_modules ]; then
  echo "Installing dependencies..."
  npm install
fi

echo "Starting backend server..."
node server.js
