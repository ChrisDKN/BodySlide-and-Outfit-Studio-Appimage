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

exec "$ROOT/bin/@BIN@" "$@"
