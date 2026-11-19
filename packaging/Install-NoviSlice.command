#!/bin/bash
# Installs NoviSlice for the current user. Double-click (or right-click > Open the first time).
cd "$(dirname "$0")" || exit 1

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

rm -rf "$VST3_DIR/NoviSlice.vst3" "$AU_DIR/NoviSlice.component"
cp -R NoviSlice.vst3 "$VST3_DIR/"
cp -R NoviSlice.component "$AU_DIR/"

# The build is not notarised by Apple, so clear the download quarantine flag.
xattr -dr com.apple.quarantine "$VST3_DIR/NoviSlice.vst3" "$AU_DIR/NoviSlice.component" 2>/dev/null
killall -9 AudioComponentRegistrar 2>/dev/null

echo ""
echo "NoviSlice installed:"
echo "  $VST3_DIR/NoviSlice.vst3"
echo "  $AU_DIR/NoviSlice.component"
echo ""
echo "Next: in FL Studio go to Options > Manage plugins > Find installed plugins, then add NoviSlice to the Channel Rack."
