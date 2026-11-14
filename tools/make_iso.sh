#!/bin/sh
#
# Build a bootable Apollo PS2 disc image with the offline saves database
# (apollo-saves) and the save patches (apollo-patches).
#
# usage: tools/make_iso.sh <APOLLO.ELF> <apollo-saves dir> <apollo-patches dir> <output.iso>
#
# Disc layout (what the app reads through cdfs:/):
#   SYSTEM.CNF                      BOOT2 = cdrom0:\APOLLO.ELF;1
#   APOLLO.ELF
#   APOLLO/PS2/games.txt            saves database (apollo-saves/PS2)
#   APOLLO/PS2/<TITLEID>/...        saves.txt, icon0.png, *.zip
#   APOLLO/PS1/...                  (apollo-saves/PS1)
#   APOLLO/PATCHES/*.savepatch      apollo-patches/PS2 + PS1, in one folder like
#                                   on the other platforms (title IDs don't overlap)
#
# Needs xorriso. The image uses Joliet names: title ID folders (9 chars) and
# the .savepatch extension don't fit plain ISO9660, and the PS2 CDFS driver
# reads the Joliet tree.

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

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

mkdir -p "$STAGE/APOLLO/PATCHES"
cp "$ELF" "$STAGE/APOLLO.ELF"
printf 'BOOT2 = cdrom0:\\APOLLO.ELF;1\r\nVER = 1.00\r\nVMODE = NTSC\r\n' > "$STAGE/SYSTEM.CNF"

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

# repository docs, not part of the database
find "$STAGE/APOLLO" -name README.md -delete

xorriso -as mkisofs -quiet -iso-level 3 -J -joliet-long -l \
	-V APOLLO_PS2 -o "$OUT" "$STAGE"

echo "$OUT: $(du -h "$OUT" | cut -f1)," \
	"$(grep -c '=' "$STAGE/APOLLO/PS2/games.txt") PS2 + $(grep -c '=' "$STAGE/APOLLO/PS1/games.txt") PS1 games," \
	"$(ls "$STAGE/APOLLO/PATCHES" | wc -l | tr -d ' ') patches"
