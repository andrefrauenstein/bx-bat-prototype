#!/bin/bash
# Installs bx_BAT EQ (VST3 + AU) for the current user and clears the download quarantine.
cd "$(dirname "$0")"
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
rm -rf ~/Library/Audio/Plug-Ins/VST3/"bx_BAT EQ.vst3" ~/Library/Audio/Plug-Ins/Components/"bx_BAT EQ.component"
cp -R "bx_BAT EQ.vst3" ~/Library/Audio/Plug-Ins/VST3/
cp -R "bx_BAT EQ.component" ~/Library/Audio/Plug-Ins/Components/
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"bx_BAT EQ.vst3" 2>/dev/null
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"bx_BAT EQ.component" 2>/dev/null
killall -9 AudioComponentRegistrar 2>/dev/null
echo ""
echo "Installed bx_BAT EQ (VST3 + AU)."
echo "In Ableton: Settings > Plug-ins > Rescan, then find it under Plug-ins > Brainworx Prototype."
echo "You can close this window."
