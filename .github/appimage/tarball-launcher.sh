#!/bin/sh
#
# Launcher for the portable (tarball) build of BodySlide and Outfit Studio.
# @BIN@ is substituted at package time; one copy of this script is installed
# per program at the root of the extracted directory.
#
# The sharun tree underneath is relocatable -- its .env refers to ${SHARUN_DIR},
# which sharun resolves from its own path -- so the extracted directory can be
# moved anywhere, including onto another machine.

set -e

# Resolve through symlinks so the tarball root is found no matter how the
# launcher was invoked.
SELF=$0
while [ -L "$SELF" ]; do
	link=$(readlink "$SELF")
	case $link in
		/*) SELF=$link ;;
		*)  SELF=$(dirname "$SELF")/$link ;;
	esac
done
ROOT=$(cd -- "$(dirname -- "$SELF")" && pwd)

# Unlike the AppImage, this directory is writable, so it doubles as the data
# directory: Config.xml, SliderSets, ShapeData and the logs all live here.
# A mod manager that keeps one shared install and several game instances
# overrides BSOS_APPDIR to separate them.
if [ -z "$BSOS_APPDIR" ]; then
	BSOS_APPDIR=$ROOT
fi
export BSOS_APPDIR

# Start sibling programs (BodySlide's "Outfit Studio" button) through sharun so
# they get the bundled libraries, rather than exec'ing the raw ELF directly.
export BSOS_BINDIR="$ROOT/bin"

# bin/ holds helper executables the bundle spawns by name rather than by path --
# notably glycin's image loaders (glycin-svg and friends), which GTK invokes to
# decode icon theme SVGs. They are spawned with execvp, which resolves against
# this process's PATH, so without this GTK aborts the moment it has to render an
# icon it cannot find. The AppImage's AppRun does the same thing.
export PATH="$ROOT/bin:$PATH"

# Host desktops (Cinnamon/Mint and friends) export GTK_MODULES pointing at
# their own GTK modules. The bundled GTK cannot load them and warns once per
# module on startup; they are desktop-integration extras, so drop them. Same
# reasoning as the AppImage's AppRun.
unset GTK_MODULES GTK3_MODULES

exec "$ROOT/bin/@BIN@" "$@"
