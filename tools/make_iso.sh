#!/bin/sh
#
# Build a bootable Apollo PS2 disc image with the offline saves database
# (apollo-saves) and the save patches (apollo-patches).
#
# usage: tools/make_iso.sh <APOLLO.ELF> <apollo-saves dir> <apollo-patches dir> <output.iso>
#
# Disc layout (what the app reads through cdfs:/):
#   SYSTEM.CNF                      BOOT2 = cdrom0:\APOL_999.99;1
#   APOL_999.99                     the app (APOLLO.ELF), named like a disc ID
#   APOLLO/PS2/games.txt            saves database (apollo-saves/PS2)
#   APOLLO/PS2/<TITLEID>/...        saves.txt, icon0.png, *.zip
#   APOLLO/PS1/...                  (apollo-saves/PS1)
#   APOLLO/PATCHES/*.savepatch      apollo-patches/PS2 + PS1, in one folder like
#                                   on the other platforms (title IDs don't overlap)
#   APOLLO/PATCHES/python/*.py      apollo-patches/python, the modules [python:]
#                                   codes import (like APOLLO/DATA/python on USB)
#
# Needs xorriso. The image uses Joliet names: title ID folders (9 chars) and
# the .savepatch extension don't fit plain ISO9660, and the PS2 CDFS driver
# reads the Joliet tree.
#
# Master disc: if ps2-master-patcher (https://github.com/bucanero/ps2-master-patcher)
# is in the PATH, or PS2_MASTER_PATCHER points to it, the image gets the
# encrypted PS2 logo (sectors 0-11) and the master disc sectors (14-15) for
# APOL-99999, so a PS2 takes it as a game disc. Both are derived from the boot
# file name, which is why it has the disc ID format. MASTER_REGION (J, U, E or
# W) sets the region; the patcher's default is USA.

set -e

if [ $# -ne 4 ]; then
	echo "usage: $0 <APOLLO.ELF> <apollo-saves dir> <apollo-patches dir> <output.iso>" >&2
	exit 1
fi

ELF=$1
SAVES=$2
PATCHES=$3
OUT=$4

for p in PS2 PS1; do
	if [ ! -f "$SAVES/$p/games.txt" ]; then
		echo "error: $SAVES/$p/games.txt not found" >&2
		exit 1
	fi
	if [ ! -d "$PATCHES/$p" ]; then
		echo "error: $PATCHES/$p not found" >&2
		exit 1
	fi
done
if [ ! -d "$PATCHES/python" ]; then
	echo "error: $PATCHES/python not found" >&2
	exit 1
fi

# Disc ID: 4 letters, then 5 digits split as NNN.NN
BOOT_ELF=APOL_999.99

PATCHER=${PS2_MASTER_PATCHER:-$(command -v ps2-master-patcher || true)}

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

mkdir -p "$STAGE/APOLLO/PATCHES"
cp "$ELF" "$STAGE/$BOOT_ELF"
printf 'BOOT2 = cdrom0:\\%s;1\r\nVER = 1.00\r\nVMODE = NTSC\r\n' "$BOOT_ELF" > "$STAGE/SYSTEM.CNF"

for p in PS2 PS1; do
	cp -R "$SAVES/$p" "$STAGE/APOLLO/$p"
	for f in "$PATCHES/$p/"*.savepatch; do
		if [ -e "$STAGE/APOLLO/PATCHES/${f##*/}" ]; then
			echo "error: ${f##*/} is in both PS1 and PS2" >&2
			exit 1
		fi
		cp "$f" "$STAGE/APOLLO/PATCHES/"
	done
done

cp -R "$PATCHES/python" "$STAGE/APOLLO/PATCHES/python"

# repository docs, not part of the database
find "$STAGE/APOLLO" -name README.md -delete

xorriso -as mkisofs -quiet -iso-level 3 -J -joliet-long -l \
	-V APOLLO_PS2 -o "$OUT" "$STAGE"

if [ -n "$PATCHER" ]; then
	# the patcher saves the sectors it replaces to its working directory
	OUT_ABS=$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")
	(cd "$STAGE" && "$PATCHER" "$OUT_ABS" $MASTER_REGION)
else
	echo "note: ps2-master-patcher not found, the image has no PS2 logo or master disc sectors"
fi

echo "$OUT: $(du -h "$OUT" | cut -f1)," \
	"$(grep -c '=' "$STAGE/APOLLO/PS2/games.txt") PS2 + $(grep -c '=' "$STAGE/APOLLO/PS1/games.txt") PS1 games," \
	"$(ls "$STAGE/APOLLO/PATCHES/"*.savepatch | wc -l | tr -d ' ') patches," \
	"$(ls "$STAGE/APOLLO/PATCHES/python/"*.py | wc -l | tr -d ' ') Python modules"
