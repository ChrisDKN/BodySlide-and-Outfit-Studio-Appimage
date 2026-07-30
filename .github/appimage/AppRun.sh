#!/bin/sh
#
# AppRun for the BodySlide and Outfit Studio AppImage.
#
# Pre-created before quick-sharun runs so that quick-sharun's _add_apprun()
# keeps it (it only writes a generic AppRun.sh when none exists). sharun itself
# is hardlinked to $APPDIR/AppRun and execs this script.
#
# Two jobs beyond the stock AppRun:
#   1. Point the programs at a writable data directory. Both derive every path
#      they use -- Config.xml, Log_*.txt, SliderSets, ShapeData, SliderPresets --
#      from one directory, which without an override is the directory holding
#      the executable. In an AppImage that is the read-only squashfs mount, so
#      config saves fail and a mod manager has nowhere to deploy outfits.
#   2. Export BSOS_BINDIR so BodySlide's "Outfit Studio" button starts the
#      bundled OutfitStudio through sharun rather than poking at the data dir.

if [ "$APPRUN_DEBUG" = 1 ]; then
	set -x
fi

set -e

MAIN_BIN=BodySlide
ARG0="${ARGV0:-$0}"
unset ARGV0

export PATH="$APPDIR/bin:$PATH"
export ARG0 APPDIR PATH

# Allow users to set env variables for a specific AppImage.
# This feature only works with the uruntime.
if [ "$1" = '--appimage-add-env' ]; then
	shift
	for v do
		echo "$v" >> "$APPIMAGE".env
		>&2 echo "Added '$v' to $APPIMAGE.env"
	done
	exit 0
fi

if [ -f "$APPDIR"/AppRun.lib ]; then
	. "$APPDIR"/AppRun.lib
	for hook in "$APPDIR"/bin/*.hook; do
		[ -e "$hook" ] || continue
		. "$hook"
	done
fi

# ---------------------------------------------------------------------------
# Host GTK modules
# ---------------------------------------------------------------------------
# Desktops such as Cinnamon/Mint and some GNOME setups export GTK_MODULES
# (colorreload-gtk-module, window-decorations-gtk-module, ...). Those are host
# .so files built against the host's GTK; the bundled GTK looks for them under
# its own GTK_PATH, does not find them, and prints "Failed to load module" for
# each one. They are desktop-integration extras the bundle cannot use in any
# case, so drop them rather than emit a warning per module on every launch.
# anylinux.so does not cover this: it clears bundle variables leaking *out* to
# child processes, whereas GTK_MODULES arrives from the host.
unset GTK_MODULES GTK3_MODULES

# ---------------------------------------------------------------------------
# Writable data directory
# ---------------------------------------------------------------------------
# A mod manager (or anyone wanting several parallel setups) overrides this to
# get a per-instance directory out of a single AppImage.
if [ -z "$BSOS_APPDIR" ]; then
	BSOS_APPDIR="${XDG_DATA_HOME:-${HOME:-/tmp}/.local/share}/BodySlide"
fi
export BSOS_APPDIR
export BSOS_BINDIR="$APPDIR/bin"

SHAREDIR="$APPDIR/share/BodySlide"

mkdir -p "$BSOS_APPDIR"

# res/ and lang/ stay in the read-only image and are symlinked in, so that an
# updated AppImage always wins over a stale copy. The mount point changes on
# every run, which leaves the previous run's symlink dangling -- that is why
# these are refreshed unconditionally. A real directory in their place is
# assumed to be deliberate and left alone.
for d in res lang; do
	if [ -L "$BSOS_APPDIR/$d" ] || [ ! -e "$BSOS_APPDIR/$d" ]; then
		ln -sfn "$SHAREDIR/$d" "$BSOS_APPDIR/$d"
	fi
done

# Seed the XML config on first run only. After that the programs own these
# files and rewrite them on exit, so copying again would discard user settings.
for f in Config.xml BodySlide.xml OutfitStudio.xml BuildSelection.xml RefTemplates.xml; do
	if [ ! -e "$BSOS_APPDIR/$f" ] && [ -f "$SHAREDIR/defaults/$f" ]; then
		cp "$SHAREDIR/defaults/$f" "$BSOS_APPDIR/$f"
		chmod u+w "$BSOS_APPDIR/$f"
	fi
done

# NOTE: do NOT pre-create SliderSets/ShapeData/SliderPresets/... here.
# Their existence is a signal, not just storage: ProjectUtil::GetProjectPath()
# treats "does <data dir>/SliderSets exist?" as "this directory is the project
# directory" and returns immediately, ahead of the game data path. An empty
# SliderSets therefore hijacks discovery and BodySlide silently lists nothing,
# instead of finding the outfits a mod manager deployed to
# <GameData>/CalienteTools/BodySlide.
#
# Leaving them absent keeps auto-discovery working. Anyone wanting a
# self-contained setup creates SliderSets themselves (or a mod manager does),
# which then deliberately opts into the data dir being the project directory.

# ---------------------------------------------------------------------------
# Pick the program to run
# ---------------------------------------------------------------------------
# --outfit-studio is a friendly alias for the bare "OutfitStudio" argument that
# the name-matching below already understands.
if [ "$1" = '--outfit-studio' ] || [ "$1" = '--outfitstudio' ]; then
	shift
	set -- OutfitStudio "$@"
fi

# Match ARG0 (set when the AppImage is symlinked or renamed to a program name),
# then an explicit first argument, then fall back to BodySlide.
if [ -f "$APPDIR"/bin/"${ARG0##*/}" ]; then
	TO_LAUNCH="$APPDIR/bin/${ARG0##*/}"
elif [ -n "$1" ] && [ -f "$APPDIR"/bin/"$1" ]; then
	TO_LAUNCH="$APPDIR/bin/$1"
	shift
else
	TO_LAUNCH="$APPDIR/bin/$MAIN_BIN"
fi

set -- "$TO_LAUNCH" "$@"

# If LD_DEBUG=libs is set outside the AppImage the output is not helpful
# because it will include the libs of sh, grep, cat, etc from the hooks
# with this var we can set LD_DEBUG=libs for the bundled application only
if [ "$APPIMAGE_DEBUG" = 1 ]; then
	cat /etc/os-release >"$PWD"/"${APPIMAGE##*/}"-debug.log || :
	export LD_DEBUG=libs
	export VK_LOADER_DEBUG=all
	export LIBGL_DEBUG=verbose
	export EGL_LOG_LEVEL=debug
	export LC_ALL=C
	export SHARUN_PRINTENV=1
	"$@" 2>>"$PWD"/"${APPIMAGE##*/}"-debug.log || :
	>&2 echo "Debug log at: '$PWD/${APPIMAGE##*/}-debug.log'"
else
	exec "$@"
fi
