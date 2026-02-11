#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

DOTNET_WIN_DEFAULT="/mnt/c/Program Files/dotnet"
DOTNET_WIN_PATH="${DOTNET_WIN_PATH:-$DOTNET_WIN_DEFAULT}"

if [[ ! -d "$DOTNET_WIN_PATH" ]]; then
  echo "[error] Windows dotnet path not found: $DOTNET_WIN_PATH"
  echo "        Set DOTNET_WIN_PATH to your Windows dotnet folder and retry."
  exit 1
fi

if [[ ! -x "$DOTNET_WIN_PATH/dotnet.exe" ]]; then
  echo "[error] dotnet.exe not found at: $DOTNET_WIN_PATH/dotnet.exe"
  exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "[error] cmake is required but not found in PATH"
  exit 1
fi

if ! command -v i686-w64-mingw32-objdump >/dev/null 2>&1; then
  echo "[error] i686-w64-mingw32-objdump is required but not found in PATH"
  exit 1
fi

export PATH="$DOTNET_WIN_PATH:$PATH"

echo "[info] Using dotnet: $(command -v dotnet.exe)"
echo "[info] Working directory: $SCRIPT_DIR"

pushd "$SCRIPT_DIR" >/dev/null

echo "[step] Cleaning stale managed client library artifacts"
rm -f out/build/windows-x86-v097/MUnique.Client.Library.dll
rm -f out/build/windows-x86-v097/MUnique.Client.Library.deps.json
rm -f out/build/windows-x86-v097/MUnique.Client.Library.pdb
rm -f out/build/windows-x86-v097/MUnique.Client.Library.xml

echo "[step] Configuring CMake preset: windows-x86-v097-release"
cmake --preset windows-x86-v097-release

echo "[step] Building preset: windows-x86-v097-release (Release)"
cmake --build --preset windows-x86-v097-release --config Release

DLL_PATH="out/build/windows-x86-v097/MUnique.Client.Library.dll"
if [[ ! -f "$DLL_PATH" ]]; then
  echo "[error] Expected DLL not found: $DLL_PATH"
  exit 1
fi

echo "[step] Verifying native export: ConnectionManager_Connect"
if i686-w64-mingw32-objdump -p "$DLL_PATH" | grep -q "ConnectionManager_Connect"; then
  echo "[ok] Native export found in $DLL_PATH"
else
  echo "[error] Missing ConnectionManager_Connect export in $DLL_PATH"
  echo "        DLL is still managed-only or NativeAOT export failed."
  exit 1
fi

echo "[done] v0.97 NativeAOT client library rebuild succeeded."

popd >/dev/null
