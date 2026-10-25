#!/bin/sh
# Cross-compiles RSFX for 32-bit ARM MPC OS, checks the binary, and builds dist/RSFX-<ver>-mpc-armv7.zip.
set -e
cd "$(dirname "$0")/.."
VER=1.1.0; NAME="RSFX-$VER"
RE="${READELF:-arm-linux-gnueabihf-readelf}"
make arm
SO=build/arm/rsfx.so
command -v "$RE" >/dev/null || { echo "ERROR: $RE not found"; exit 1; }
"$RE" -h "$SO" | grep -q 'ELF32' || { echo "ERROR: not a 32-bit ELF"; exit 1; }
"$RE" -h "$SO" | grep -q 'ARM' || { echo "ERROR: not an ARM binary"; exit 1; }
"$RE" -A "$SO" | grep -q 'Tag_ABI_VFP_args: VFP registers' || { echo "ERROR: not hard-float"; exit 1; }
"$RE" --dyn-syms -W "$SO" | grep -q ' VSTPluginMain$' || { echo "ERROR: VSTPluginMain not exported"; exit 1; }
"$RE" --dyn-syms -W "$SO" | grep -q ' main$' || { echo "ERROR: main entry not exported"; exit 1; }
needed=$("$RE" -d "$SO" | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p' | tr '\n' ' ')
echo "needs: $needed"
for lib in $needed; do case "$lib" in libc.so.6|libm.so.6|ld-linux-armhf.so.3) ;; *) echo "ERROR: unexpected dependency $lib"; exit 1 ;; esac; done
maxg=$("$RE" --dyn-syms -W "$SO" | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1)
echo "highest glibc symbol: ${maxg:-none}"
if [ -n "$maxg" ] && [ "$(printf '%s\n' "$maxg" GLIBC_2.34 | sort -V | tail -1)" != GLIBC_2.34 ]; then
    echo "ERROR: needs $maxg, newer than the JV-880 package (GLIBC_2.34); build on an older distro"; exit 1
fi
SKIN="mpc/skin/RSFX - VST - RSFX"
python3 tools/make_skin.py
python3 tools/check_skin.py
rm -rf "dist/$NAME"; mkdir -p "dist/$NAME/payload/vst" "dist/$NAME/payload/Synths"
cp -a "$SKIN" "dist/$NAME/payload/Synths/"
cp "$SO" "dist/$NAME/payload/vst/rsfx.so"
cp mpc/install.sh mpc/uninstall.sh mpc/plugin.xml mpc/plugin_list.awk mpc/INSTALL.md "dist/$NAME/"
chmod +x "dist/$NAME/"*.sh
(cd "dist/$NAME" && sha256sum install.sh uninstall.sh plugin.xml plugin_list.awk payload/vst/rsfx.so \
    "payload/Synths/RSFX - VST - RSFX/Plugin Skins/TUI.json" > SHA256SUMS)
(cd dist && rm -f "$NAME-mpc-armv7.zip" && zip -qr "$NAME-mpc-armv7.zip" "$NAME")
echo "OK: dist/$NAME-mpc-armv7.zip"
