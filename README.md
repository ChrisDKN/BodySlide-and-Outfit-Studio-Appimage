BodySlide and Outfit Studio
===========================

BodySlide and Outfit Studio, a tool to convert, create, and customize outfits and bodies for The Elder Scrolls, Fallout and Starfield.

[![CMake Release](https://github.com/ousnius/BodySlide-and-Outfit-Studio/actions/workflows/cmake-release.yml/badge.svg)](https://github.com/ousnius/BodySlide-and-Outfit-Studio/actions/workflows/cmake-release.yml)

## About this fork

This fork adds portable Linux builds (an AppImage and a relocatable `.tar.zst`, both
produced by the [Linux Release](.github/workflows/linux-release.yml) workflow) and a set
of environment variables so a launcher or mod manager can drive the programs without the
user having to configure anything in the GUI.

### Packages

Each release publishes two x86_64 artifacts. Both come out of a single deployment, so
they bundle exactly the same libraries and behave identically once running - only the
packaging differs:

| Artifact | Use it when |
| --- | --- |
| `BodySlide-and-Outfit-Studio-<version>-x86_64.AppImage` | Running it yourself on a desktop. One file, nothing to extract, updatable through the published `.zsync`. |
| `BodySlide-and-Outfit-Studio-<version>-x86_64.tar.zst` | Driving it from a mod manager, or on any host where the AppImage cannot mount itself. |

Neither needs wxWidgets, GLEW or mesa installed. The bundles carry their own loader and
glibc, so the (bleeding-edge) glibc they are built against does not become the
compatibility floor: they run on SteamOS, Arch, Debian, Fedora and Ubuntu alike.

Both bundles ship BodySlide and Outfit Studio together rather than as two packages -
BodySlide can launch Outfit Studio, and keeping them in one bundle both halves the
download and lets that button keep working.

#### The portable tarball

Prefer this one for mod managers, and for sandboxed or locked-down hosts.

The AppImage mounts itself with FUSE, which is not available inside a Flatpak sandbox
(no `/dev/fuse`, no `fusermount3`), in minimal containers, or on hosts where `/tmp` is
mounted `noexec`. The tarball is a plain directory tree and has no such requirement.

It is also the better shape to deploy outfits into: the extracted root is writable and
doubles as the data directory, so `SliderSets/`, `ShapeData/`, `Config.xml` and the logs
all live in one place, with no read-only mount and no writable-directory indirection.

```sh
tar --zstd -xf BodySlide-and-Outfit-Studio-*.tar.zst
cd BodySlide-and-Outfit-Studio-*-x86_64/
./BodySlide          # or ./OutfitStudio
```

`tar --zstd` needs the `zstd` program on `PATH`; without it, use
`zstd -dc BodySlide-and-Outfit-Studio-*.tar.zst | tar -x` instead.

The extracted directory looks like this:

```
BodySlide-and-Outfit-Studio-<version>-x86_64/
├── BodySlide             launcher script - start here
├── OutfitStudio          launcher script
├── bin/                  the real executables, plus bundled helpers
├── res/  lang/           shaders, XRC layouts, reference meshes, translations
├── Config.xml  ...       the five XML config files, seeded from the defaults
└── (bundled libraries)
```

Start the programs through the two launcher scripts at the root, not the binaries in
`bin/` - the launchers are what set up `BSOS_APPDIR`, `BSOS_BINDIR` and `PATH` for the
bundle. They resolve their own location through symlinks, so a symlink from
`~/.local/bin/BodySlide` works, and the whole tree is relocatable: move it anywhere, or
copy it to another machine, and it still runs.

The tarball deliberately ships **without** empty `SliderSets`/`ShapeData` directories.
See [A note on project discovery](#a-note-on-project-discovery) below before creating
them - their presence changes where outfits are looked for.

#### If the AppImage will not start

Run it with `--appimage-extract-and-run` (or set `URUNTIME_EXTRACT_AND_RUN=1`). That
skips the FUSE mount and unpacks to a temporary directory instead, at the cost of a
slower start. If that also fails, the host most likely has `/tmp` mounted `noexec` or the
AppImage sits on a filesystem that cannot carry the executable bit (NTFS, exFAT) - use
the tarball.

### Environment variables

| Variable | Purpose |
| --- | --- |
| `BSOS_APPDIR` | Data directory: `Config.xml`, `Log_BS.txt` / `Log_OS.txt`, `res/`, `lang/`, and - when it contains a `SliderSets` directory - the project data. Defaults to the directory holding the executable, which is what the tarball launchers use (the extracted root); the AppImage defaults it to `${XDG_DATA_HOME:-~/.local/share}/BodySlide` instead, because its own directory is a read-only mount. |
| `BSOS_BINDIR` | Directory to launch sibling executables from, used by BodySlide's "Outfit Studio" button. Set this when the data directory holds no executables (AppImage) or when the programs must be started through a wrapper rather than as raw ELF binaries. Defaults to the executable's own directory. |
| `BSOS_TARGET_GAME` | Game to target. Accepts a name from the list below (case-insensitive) or the raw index. An unrecognised value is logged as a warning and ignored, rather than silently selecting the wrong game. |
| `BSOS_GAME_DATA_PATH` | The game's `Data` directory. Also written to the per-game slot the settings dialog keeps, so switching game in the UI and back does not lose it. |
| `BSOS_OUTPUT_DATA_PATH` | Where built meshes are written. Defaults to `BSOS_GAME_DATA_PATH`; set it separately to capture build output into a mod folder instead of dropping it into the game. |

Valid `BSOS_TARGET_GAME` names (index in parentheses): `Fallout3` (0),
`FalloutNewVegas` (1), `Skyrim` (2), `Fallout4` (3), `SkyrimSpecialEdition` (4),
`Fallout4VR` (5), `SkyrimVR` (6), `Fallout76` (7), `Oblivion` (8), `Starfield` (9).

The three game/path variables are applied at startup, before anything reads the
configuration, and **win over the stored settings on every launch**. A value edited in
the settings dialog is therefore reverted the next time the launcher starts the program.
That is deliberate: the launcher is authoritative. Leave a variable unset to let the user
control that setting through the GUI as usual.

### Examples

Run against a Skyrim SE install, writing build output into a mod folder:

```sh
export BSOS_TARGET_GAME=SkyrimSpecialEdition
export BSOS_GAME_DATA_PATH="$HOME/Games/Skyrim Special Edition/Data"
export BSOS_OUTPUT_DATA_PATH="$HOME/mods/BodySlide Output"
./BodySlide-and-Outfit-Studio-*.AppImage
```

Two instances out of one AppImage, each with its own settings and logs:

```sh
BSOS_APPDIR="$HOME/.local/share/BodySlide/skyrim" \
BSOS_TARGET_GAME=SkyrimSpecialEdition \
BSOS_GAME_DATA_PATH="$HOME/Games/Skyrim Special Edition/Data" \
	./BodySlide-and-Outfit-Studio-*.AppImage
```

Start Outfit Studio instead of BodySlide - pass `--outfit-studio` (or `OutfitStudio`) as
the first argument, or symlink/rename the AppImage to `OutfitStudio`:

```sh
./BodySlide-and-Outfit-Studio-*.AppImage --outfit-studio
```

The examples use the AppImage, but the variables work the same way for the tarball -
substitute `./BodySlide` for the AppImage in each one. The tarball has no
`--outfit-studio` argument because it does not need one: run `./OutfitStudio` directly.
Note that its `BSOS_APPDIR` defaults to the extracted directory rather than
`~/.local/share/BodySlide`, so the two-instance example above is what you want if you
run one shared install against several games.

### A note on project discovery

Outfits are found by `ProjectUtil::GetProjectPath()`, which checks, in order: a configured
`ProjectPath`, then `<BSOS_APPDIR>/SliderSets`, then `<game data>/CalienteTools/BodySlide`
and `<game data>/Tools/BodySlide`.

The presence of `SliderSets` in the data directory is a signal, not just storage: it means
"this is the project directory", and discovery stops there. So the packages deliberately
ship **without** an empty `SliderSets` - creating one would hijack discovery and leave
BodySlide listing nothing, even when a mod manager has deployed outfits to the game's
`CalienteTools/BodySlide` folder. Create it yourself (or let a mod manager create it) only
when you actually want a self-contained setup under `BSOS_APPDIR`.

## Documentation
* [Wiki Overview](https://github.com/ousnius/BodySlide-and-Outfit-Studio/wiki)
* [Guides and Documentation](https://github.com/ousnius/BodySlide-and-Outfit-Studio/wiki/Guides-and-Documentation)

## Build Instructions
* [Building on Windows (Visual Studio or CMake)](https://github.com/ousnius/BodySlide-and-Outfit-Studio/wiki/Building-on-Windows-%28Visual-Studio-or-CMake%29)
* [Building on Linux](https://github.com/ousnius/BodySlide-and-Outfit-Studio/wiki/Building-on-Linux)
* [Installation and Settings](https://github.com/ousnius/BodySlide-and-Outfit-Studio/wiki/Installation-and-Settings)
* [Installation on Linux](https://github.com/ousnius/BodySlide-and-Outfit-Studio/wiki/Installation-on-Linux)

## Credits / Libraries
**Created by and/or with the help of:**
* Caliente
* ousnius
* jonwd7 for NIF and general help
* degenerated1123 for help with shaders
* NifTools team

**Relevant work:**
* [nifly](https://github.com/ousnius/nifly): C++ NIF library for the NetImmerse File Format (NetImmerse, Gamebryo, Creation Engine).
* [NiflySharp](https://github.com/ousnius/NiflySharp): Native C# / .NET version of nifly that uses source generation based on [nifxml](https://github.com/niftools/nifxml).

**Libraries used:**
* [wxWidgets](https://wxwidgets.org/)
* [nifly](https://github.com/ousnius/nifly)
* [OpenGL](https://www.opengl.org/)
* [OpenGL Image (GLI)](https://github.com/g-truc/gli)
* [Simple OpenGL Image Library 2 (SOIL2)](https://github.com/SpartanJ/SOIL2)
* [half - IEEE 754-based half-precision floating point library](https://half.sourceforge.net/)
* [Miniball](https://github.com/hbf/miniball)
* [LZ4(F)](https://github.com/lz4/lz4)
* [TinyXML-2](https://github.com/leethomason/tinyxml2)
* [nlohmann/json](https://github.com/nlohmann/json)
* [fkYAML](https://github.com/fktn-k/fkYAML)
* [Catch2 v3](https://github.com/catchorg/Catch2) (optional tests)
* FSEngine (BSA/BA2 library)
* [Autodesk FBX SDK](https://aps.autodesk.com/developer/overview/fbx-sdk)
