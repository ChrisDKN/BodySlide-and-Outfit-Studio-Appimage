#!/bin/bash
#
# Build the portable Linux artifacts for BodySlide and Outfit Studio:
#
#   BodySlide-and-Outfit-Studio-<version>-<arch>.AppImage   (+ .zsync)
#   BodySlide-and-Outfit-Studio-<version>-<arch>.tar.zst
#
# Both come out of a single quick-sharun deployment, so they bundle exactly the
# same libraries. sharun bundles the loader and glibc alongside the app, which
# is what makes these run on any reasonably modern distro regardless of the
# (bleeding-edge) glibc they were built against.
#
# Designed to run inside ghcr.io/pkgforge-dev/archlinux -- see
# .github/workflows/linux-release.yml. It can be run locally in that same
# container image; it will not work on a plain host without quick-sharun.
#
# Both programs are packaged into ONE AppImage rather than two: BodySlide can
# launch Outfit Studio, and keeping them in a single bundle both halves the
# download and lets that button keep working. Pick the program with the first
# argument (`BodySlide.AppImage OutfitStudio`), with `--outfit-studio`, or by
# symlinking the AppImage to the program name.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

export PATH="$HOME/.local/bin:$PATH"

ARCH="${ARCH:-$(uname -m)}"

# Prefer an explicit VERSION (the workflow passes the tag), fall back to git.
if [ -z "${VERSION:-}" ]; then
	VERSION="$(git -C "$REPO_ROOT" describe --tags --always --dirty 2>/dev/null || echo unknown)"
fi
VERSION="${VERSION#v}"

WORK_DIR="${WORK_DIR:-${TMPDIR:-/tmp}/bsos-appimage-build}"
BUILD_DIR="$WORK_DIR/build"
APPDIR="$WORK_DIR/AppDir"
OUTPATH="$WORK_DIR/dist"
STAGE_DATA="$WORK_DIR/stage-data"
FINAL_OUTPATH="${FINAL_OUTPATH:-$SCRIPT_DIR/dist}"

PKGNAME="BodySlide-and-Outfit-Studio-${VERSION}-${ARCH}"

# ── Tooling check ────────────────────────────────────────────────────
for tool in quick-sharun cmake ninja git tar zstd; do
	command -v "$tool" >/dev/null || {
		echo "ERROR: '$tool' not found in PATH" >&2
		exit 1
	}
done

echo "=== Cleaning previous build ==="
rm -rf "$WORK_DIR" "$FINAL_OUTPATH"
mkdir -p "$APPDIR" "$OUTPATH" "$STAGE_DATA" "$FINAL_OUTPATH"

# ── Compile ──────────────────────────────────────────────────────────
# FBX SDK is off: it is not redistributable, and the Windows job disables it
# too, so enabling it here would make the Linux build the odd one out.
echo "=== Configuring ==="
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
	-G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DBSOS_ENABLE_FBXSDK=OFF

echo "=== Compiling ==="
cmake --build "$BUILD_DIR" --target BodySlide OutfitStudio --parallel

for bin in BodySlide OutfitStudio; do
	[ -x "$BUILD_DIR/$bin" ] || { echo "ERROR: $bin was not built" >&2; exit 1; }
done

# ── Data directory used during deployment ────────────────────────────
# quick-sharun's strace mode launches each binary under xvfb for a few seconds
# to catch libraries that are dlopen'd rather than linked (GTK modules,
# gdk-pixbuf loaders, GL drivers). Without its resources BodySlide bails out
# during startup and that trace finds almost nothing, so point BSOS_APPDIR at a
# populated scratch directory for the duration of the build. This is the same
# override the AppRun uses at runtime.
cp -a "$REPO_ROOT/res" "$REPO_ROOT/lang" "$STAGE_DATA/"
cp "$REPO_ROOT"/{Config.xml,BodySlide.xml,OutfitStudio.xml,BuildSelection.xml,RefTemplates.xml} "$STAGE_DATA/"
mkdir -p "$STAGE_DATA"/{SliderSets,Automations,PoseData,RefTemplates,ShapeData,SliderCategories,SliderGroups,SliderPresets}
export BSOS_APPDIR="$STAGE_DATA"

# ── Stage our AppRun ─────────────────────────────────────────────────
# quick-sharun only generates its stock AppRun.sh when none is present, so
# installing ours first is what makes it survive.
install -Dm755 "$SCRIPT_DIR/AppRun.sh" "$APPDIR/AppRun.sh"

# ── quick-sharun ─────────────────────────────────────────────────────
# DEPLOY_OPENGL:  wxGLCanvas + GLEW; deploys libglvnd so the host's real GL
#                 driver is still used at runtime.
# DEPLOY_GTK/GDK: wxWidgets is built against GTK3 here; forcing these avoids
#                 depending on the ldd trace happening to reach them.
# ANYLINUX_LIB:   scrubs bundle-specific env vars from child processes, so that
#                 e.g. an external editor launched from the app is not handed
#                 our LD_LIBRARY_PATH.
# ALWAYS_SOFTWARE is deliberately NOT set -- this is a 3D preview tool and
# should use hardware acceleration.
export APPDIR OUTPATH ARCH
export DESKTOP="$SCRIPT_DIR/BodySlide.desktop"
export ICON="$REPO_ROOT/res/images/BodySlide.png"
export DEPLOY_OPENGL=1
export DEPLOY_GTK=1
export DEPLOY_GDK=1
export ANYLINUX_LIB=1
export STRACE_TIME="${STRACE_TIME:-20}"

echo "=== Running quick-sharun ==="
quick-sharun "$BUILD_DIR/BodySlide" "$BUILD_DIR/OutfitStudio"

# ── Application resources ────────────────────────────────────────────
# res/ holds the XRC layouts, shaders, images and skeleton .nif files; lang/
# holds the translations. Both are read-only at runtime and are symlinked into
# the data directory by the AppRun.
SHAREDIR="$APPDIR/share/BodySlide"
mkdir -p "$SHAREDIR/defaults"
cp -a "$REPO_ROOT/res" "$REPO_ROOT/lang" "$SHAREDIR/"
cp "$REPO_ROOT"/{Config.xml,BodySlide.xml,OutfitStudio.xml,BuildSelection.xml,RefTemplates.xml} \
	"$SHAREDIR/defaults/"

# Hicolor icon so AppImageLauncher / appimaged can install a .desktop file.
install -Dm644 "$REPO_ROOT/res/images/BodySlide.png" \
	"$APPDIR/usr/share/icons/hicolor/256x256/apps/BodySlide.png"
install -Dm644 "$REPO_ROOT/res/images/OutfitStudio.png" \
	"$APPDIR/usr/share/icons/hicolor/256x256/apps/OutfitStudio.png"

# ── Portable tarball ─────────────────────────────────────────────────
# Built from a copy of the AppDir *before* it is squashed into an AppImage.
# For a mod manager this is the friendlier artifact: it extracts to a plain
# writable directory whose SliderSets/ and ShapeData/ can be deployed into
# directly, with no FUSE mount and no writable-directory indirection.
echo "=== Building portable tarball ==="
TAR_ROOT="$WORK_DIR/tarball"
TAR_DIR="$TAR_ROOT/$PKGNAME"
mkdir -p "$TAR_ROOT"
cp -a "$APPDIR" "$TAR_DIR"

# AppImage-only bits. sharun itself must stay: it is the launcher that bin/*
# symlinks point at. AppRun is a hardlink to it, so removing AppRun is safe.
rm -f "$TAR_DIR/AppRun" "$TAR_DIR/AppRun.sh" "$TAR_DIR/.DirIcon" "$TAR_DIR"/*.desktop

# In the tarball the root directory is itself writable and doubles as the data
# directory, so res/, lang/ and the XML defaults move up out of share/.
mv "$TAR_DIR/share/BodySlide/res" "$TAR_DIR/share/BodySlide/lang" "$TAR_DIR/"
mv "$TAR_DIR/share/BodySlide/defaults"/*.xml "$TAR_DIR/"
rmdir "$TAR_DIR/share/BodySlide/defaults" "$TAR_DIR/share/BodySlide"

for bin in BodySlide OutfitStudio; do
	sed "s|@BIN@|$bin|g" "$SCRIPT_DIR/tarball-launcher.sh" > "$TAR_DIR/$bin"
	chmod 755 "$TAR_DIR/$bin"
done

mkdir -p "$TAR_DIR"/{SliderSets,Automations,PoseData,RefTemplates,ShapeData,SliderCategories,SliderGroups,SliderPresets}

tar --zstd -C "$TAR_ROOT" -cf "$FINAL_OUTPATH/${PKGNAME}.tar.zst" "$PKGNAME"

# ── AppImage ─────────────────────────────────────────────────────────
# UPINFO is set explicitly so the generated .zsync filename matches OUTNAME;
# quick-sharun would otherwise guess it from GITHUB_REPOSITORY.
echo "=== Building AppImage ==="
_gh_repo="${GITHUB_REPOSITORY:-ChrisDKN/BodySlide-and-Outfit-Studio}"
export OUTNAME="${PKGNAME}.AppImage"
export UPINFO="gh-releases-zsync|${_gh_repo%/*}|${_gh_repo#*/}|latest|*${ARCH}.AppImage.zsync"
quick-sharun --make-appimage

mv "$OUTPATH/$OUTNAME" "$FINAL_OUTPATH/"
for zs in "$OUTPATH"/*.zsync; do
	[ -e "$zs" ] && mv "$zs" "$FINAL_OUTPATH/"
done

echo
echo "=== Build complete ==="
ls -lh "$FINAL_OUTPATH"
