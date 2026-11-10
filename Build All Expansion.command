#!/bin/bash
# Epsilon Expansion: build and install all nine plugins in one go.
# Double-click this file in Finder (or run it in Terminal). It needs Xcode, nothing else.
#
# What it does, for Heat, Valve, Crush, Prism, Time, Plate, Space, Tone and Voice:
#   1. builds the Release VST3 + AU + Standalone from the Projucer Xcode project in each folder
#   2. copies the VST3 and AU into ~/Library/Audio/Plug-Ins (replacing older versions)
#   3. clears the macOS download quarantine so the DAW will load them
# A log for each plugin goes to build-logs/. Re-running only rebuilds what changed.
#
# Options (Terminal): ./"Build All Expansion.command" Heat Plate   -> build just those
#                     UNIVERSAL=1 ./"Build All Expansion.command"   -> Intel + Apple Silicon in one binary (slower)

cd "$(dirname "$0")" || exit 1
EFFECTS=(Heat Valve Crush Prism Time Plate Space Tone Voice)
[ $# -gt 0 ] && EFFECTS=("$@")

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
LOGS="$PWD/build-logs"
mkdir -p "$VST3_DIR" "$AU_DIR" "$LOGS"

finish() { echo; read -r -p "Press Return to close this window." _; exit "$1"; }

if ! xcodebuild -version >/dev/null 2>&1; then
    echo "Xcode isn't set up. Install Xcode from the App Store, open it once, then run:"
    echo "  sudo xcode-select -s /Applications/Xcode.app"
    finish 1
fi

if [ "${UNIVERSAL:-0}" = "1" ]; then ARCH_ARGS=(ARCHS="arm64 x86_64" ONLY_ACTIVE_ARCH=NO)
else ARCH_ARGS=(ONLY_ACTIVE_ARCH=YES); fi   # just this Mac's chip: about twice as fast

echo "Epsilon Expansion: building ${#EFFECTS[@]} plugins ($(uname -m), Release)"
echo "This takes a while the first time (JUCE is compiled once per plugin); later runs are quick."
echo

ok=(); failed=(); start=$SECONDS
for fx in "${EFFECTS[@]}"; do
    name="Epsilon $fx"
    project="Epsilon$fx/Builds/MacOSX/$name.xcodeproj"
    if [ ! -d "$project" ]; then echo "  ?  $fx: no project at $project"; failed+=("$fx"); continue; fi
    printf "  ...  %-6s building" "$fx"
    t0=$SECONDS
    if xcodebuild -project "$project" -target "$name - All" -configuration Release \
                  "${ARCH_ARGS[@]}" CODE_SIGN_IDENTITY="-" build > "$LOGS/$fx.log" 2>&1; then
        out="Epsilon$fx/Builds/MacOSX/build/Release"
        rm -rf "$VST3_DIR/$name.vst3" "$AU_DIR/$name.component"
        cp -R "$out/$name.vst3" "$VST3_DIR/" && cp -R "$out/$name.component" "$AU_DIR/"
        xattr -dr com.apple.quarantine "$VST3_DIR/$name.vst3" "$AU_DIR/$name.component" 2>/dev/null
        printf "\r  OK   %-6s built and installed (%ds)\n" "$fx" $((SECONDS - t0))
        ok+=("$fx")
    else
        printf "\r  FAIL %-6s see build-logs/%s.log\n" "$fx" "$fx"
        grep -m 5 -E "error:" "$LOGS/$fx.log" | sed 's/^/         /'
        failed+=("$fx")
    fi
done

# Ask macOS to re-scan Audio Units so Logic / GarageBand / FL see the new versions
killall -9 AudioComponentRegistrar >/dev/null 2>&1

echo
echo "Done in $(( (SECONDS - start) / 60 )) min: ${#ok[@]} installed${failed:+, ${#failed[@]} failed (${failed[*]})}."
echo "Installed to:  $VST3_DIR"
echo "               $AU_DIR"
echo "In FL Studio: Options > Manage plugins > Find more plugins, then look under Epsilon Sounds."
[ ${#failed[@]} -eq 0 ] && finish 0 || finish 1
