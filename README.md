# SLTextureDecoder

Decodes Second Life / Firestorm JPEG 2000 texture caches
(`texture.entries` + `texture.cache` + per-texture `.texture` files) into PNG.

> **Educational and research use only.** The tool reads a cache that is already
> on your own disk, decodes it locally and writes PNG files; it opens the cache
> read-only, never connects to the grid, never downloads anything and does not
> touch another user's data. Copyright in the textures stays with their
> creators, and this project claims no rights in them. Not affiliated with,
> endorsed by or supported by Linden Research, Inc. or the Firestorm Viewer
> project - read [NOTICE.md](NOTICE.md) before use.

## Status

| Stage | Content | State |
| --- | --- | --- |
| 0 | Toolchain, dependency check | done |
| 1 | Skeleton: `sl_texture_decoder_core` + CLI, CMake, gtest/ctest | done |
| 2 | `texture.entries` parser, cache layout, frozen test slice | done |
| 3 | Codestream assembler (header block + body) | done |
| 4 | `.texture` body discovery / batch iteration | done |
| 5 | JPEG 2000 decoding via OpenJPEG (strict + lenient fallback) | done |
| 6 | Component handling (1/2/3/4/5 components → RGBA) | done |
| 7 | PNG output via libpng | done |
| 8 | CLI (`--cache-dir`, filters, progress) | done |
| 9 | Batch conversion service (worker pool, resume) | done |
| 10 | GUI (Qt 6 Widgets, dynamic) | done |
| 11 | Robustness / error reporting | done |
| 12 | Release build, static OpenJPEG | deferred |

Stage 11 closed the robustness work: atomic output (`<name>.part` + rename), Ctrl+C
with exit code `10`, sanity limits before any allocation, and `--dry-run`. What it
left open is tracked as issues: [#3](https://github.com/mindwork64/SLTextureDecoder/issues/3)
(negative corpus and fuzzing), [#4](https://github.com/mindwork64/SLTextureDecoder/issues/4)
(machine-readable error report), [#5](https://github.com/mindwork64/SLTextureDecoder/issues/5)
(CI with sanitizers and a clang-format check) and
[#6](https://github.com/mindwork64/SLTextureDecoder/issues/6) (live cache: sharing
violations, cache changed during a run).

The reverse engineered layout, the two populations of records and the evidence
behind them are documented in [docs/format-notes.md](docs/format-notes.md).

## Documentation

The **[wiki](https://github.com/mindwork64/SLTextureDecoder/wiki)** collects the
same material in browsable pages:

* [Installation](https://github.com/mindwork64/SLTextureDecoder/wiki/Installation)
  - requirements, MSYS2 packages, build, products, where the viewer keeps its
  cache.
* [Usage](https://github.com/mindwork64/SLTextureDecoder/wiki/Usage) - every
  option, output naming, exit codes, examples.
* [GUI](https://github.com/mindwork64/SLTextureDecoder/wiki/GUI) - the window,
  the stored settings, the headless `--batch` mode.
* [Cache format](https://github.com/mindwork64/SLTextureDecoder/wiki/Cache-Format)
  - the `texture.*` layout, complete vs truncated records, component mapping.
* [Architecture](https://github.com/mindwork64/SLTextureDecoder/wiki/Architecture)
  - module map, decoding pipeline, threading model, tests.
* [Performance](https://github.com/mindwork64/SLTextureDecoder/wiki/Performance)
  - measured throughput, memory, tuning.
* [Troubleshooting](https://github.com/mindwork64/SLTextureDecoder/wiki/Troubleshooting)
  - exit codes, truncated records, Qt runtime, slow runs.
* [Legal notice](https://github.com/mindwork64/SLTextureDecoder/wiki/Legal-Notice)
  - scope of use, rights in the output, trademarks, licence.

## Requirements

### Windows (MSYS2)

* MSYS2 `mingw-w64-x86_64` toolchain, C++20
* CMake ≥ 3.16 and Ninja
* `mingw-w64-x86_64-openjpeg2` (2.5.4), `mingw-w64-x86_64-libpng` (1.6.58)
* `mingw-w64-x86_64-gtest` (1.17.0) and `mingw-w64-x86_64-cmake`
* optional, for the GUI: `mingw-w64-x86_64-qt6-base`

```sh
pacman -S --needed mingw-w64-x86_64-{toolchain,cmake,ninja,openjpeg2,libpng,gtest,qt6-base}
```

### Linux (Debian / Ubuntu)

The same tree builds against the distribution packages - verified on Ubuntu
24.04 / Linux Mint 22 with GCC 13, CMake 3.28 and Ninja.

* `g++`, `cmake`, `ninja-build` - toolchain, C++20
* `libopenjp2-7-dev` - OpenJPEG 2.5 headers and CMake package
* `libpng-dev` - libpng 1.6
* `libgtest-dev` - GoogleTest, for the unit tests
* optional, for the GUI: `qt6-base-dev`

```sh
sudo apt-get install -y g++ cmake ninja-build libopenjp2-7-dev libpng-dev libgtest-dev qt6-base-dev
```

The Debian/Ubuntu OpenJPEG package ships `OpenJPEGConfig.cmake` without a matching
`OpenJPEGConfigVersion.cmake`, so a versioned `find_package(OpenJPEG 2.5)` is
rejected as "version: unknown". `CMakeLists.txt` therefore tries the versioned
form first and falls back to the unversioned one.

Without Qt 6 the GUI target is skipped silently; `-DSLTCD_BUILD_GUI=OFF` turns it
off explicitly.

## Build & test

```sh
# Windows (MSYS2 mingw-w64 shell)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
cmake --build build
ctest --test-dir build --output-on-failure

# Linux: the system packages are found automatically, no prefix path needed
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Products (`.exe` on Windows, no suffix on Linux):

* `build/bin/SLTextureDecoder[.exe]` – the CLI
* `build/bin/SLTextureDecoderGUI[.exe]` – the Qt 6 desktop frontend (in a Windows
  build it needs the `C:\msys64\mingw64\bin` directory, or `windeployqt`, on
  `PATH`; on Linux the distribution Qt libraries)
* `build/bin/sl_texture_decoder_tests[.exe]` – the unit tests
* `build/lib/libsl_texture_decoder_core.a` – the decoding library

## Usage

```sh
SLTextureDecoder --cache-dir <dir> [options]
```

| Option | Meaning |
| --- | --- |
| `--cache-dir <dir>` | cache directory to read (required) |
| `--out-dir <dir>` | where the PNG files go (default `<cache-dir>/png`) |
| `--id <uuid>` | decode one texture instead of the whole cache |
| `--index <n>` | decode only the record at this index |
| `--limit <n>` | stop after n textures |
| `--complete-only` | skip records whose cached codestream is truncated |
| `--overwrite` | write over existing PNG files (default: skip them) |
| `--dry-run` | report what a run would write and change nothing |
| `--keep-j2k` | also write the assembled codestream as `<uuid>.j2c` |
| `--no-alpha` | write an RGB PNG (drop the alpha channel) |
| `-v`, `--verbose` | log every record |
| `-h`, `--help`, `-V`, `--version` | help / version |

Each texture becomes `<uuid>.png` (complete codestream) or `<uuid>.partial.png`
(best effort from a truncated one - see
[docs/format-notes.md](docs/format-notes.md)). With `--keep-j2k` the assembled
codestream is stored next to it as `<uuid>.j2c` / `<uuid>.partial.j2c`, and
`--no-alpha` writes RGB instead of RGBA. The exit code is `0` when every
selected texture was converted, otherwise the code of the first failure
(`3` entry not found, `6` decode failure, `10` the run was interrupted, ... -
see `utils/Errors.h`).

`--dry-run` reads `texture.entries` plus the per-texture body files (name and
size, no decoding) and reports the numbers a real run would produce, so it can
gate a long run without touching the output directory at all.

Robustness: every file is written to `<name>.part` first and renamed over the
target once it is complete, so an interrupted run leaves a stale `.part` file
instead of a truncated PNG - Ctrl+C stops after the record in progress and
exits with code `10`. A broken record is logged and counted instead of ending
the run (unexpected `std::bad_alloc` and other `std::exception`s included), and
a record that announces a body or an image size no texture of this format can
have is refused before anything is allocated (see `utils/Constants.h`).

```powershell
# five textures into a scratch directory, with per record logging
.\build\bin\SLTextureDecoder.exe --cache-dir 'D:\FS Cache\texturecache' `
    --out-dir "$env:TEMP\out" --limit 5 --verbose

# only the records that hold the whole codestream
.\build\bin\SLTextureDecoder.exe --cache-dir 'D:\FS Cache\texturecache' `
    --out-dir D:\out --complete-only
```

Cost is dominated by OpenJPEG itself and the viewer writes single tile
codestreams, so the decoder cannot spread one texture over several cores: expect
roughly 0.05 s for a 256×256 and 0.3 s for a 1024×1024 texture. A full 54 825
texture cache therefore takes hours with this single threaded CLI - use
`--limit`, `--complete-only` or `--id` while exploring, and the GUI or
`--batch --jobs <n>` for a full run.

## GUI

`SLTextureDecoderGUI.exe` wraps the same library in a window: pick the cache
folder (the record count and the encoder name of the cache appear right away),
pick the output folder (empty means `<cache>/png`), optionally limit the run,
then press **Start conversion**. The window shows a progress bar, the log with
warnings and errors in colour, the list of converted files and a preview of the
selected one; **Cancel** stops after the texture that is being decoded.

* **Threads** (`auto` = one per logical CPU) decodes several textures in
  parallel. The viewer writes single tile codestreams, so this is the only way
  to use more than one core; on a 2 core / 4 thread CPU a `--jobs 4` run was
  measured at about 2.5x a single threaded one (120 textures: 17.5 s vs 44.3 s),
  with byte identical output. Numbers are on the
  [wiki Performance page](https://github.com/mindwork64/SLTextureDecoder/wiki/Performance).
* **Overwrite existing files** stays off by default, so the PNGs already in the
  output folder are skipped: an interrupted run is resumed by starting it again.
* **Keep J2K codestream** stores the assembled codestream as `<uuid>.j2c` next to
  each PNG; **No alpha channel** writes RGB PNGs instead of RGBA. Both start off
  and are remembered with the other options.
* The last used folders and options are remembered in
  `HKCU\Software\SLTextureDecoder\GUI`.
* Browse the results in Explorer with **Open output folder**.
* The status bar carries a permanent reminder that the tool is for educational
  use and is not affiliated with Linden Lab or the Firestorm project (details in
  [NOTICE.md](NOTICE.md)).

The same binary converts without a window, which is what an unattended overnight
run uses:

```powershell
.\build\bin\SLTextureDecoderGUI.exe --batch --cache-dir 'D:\FS Cache\texturecache' `
    --out-dir D:\out --jobs 4 --complete-only
```

`--batch` accepts every CLI option plus `--jobs <n>`, and it returns the exit
code of the CLI (0 when nothing failed).

## Layout

```
src/
  VersionInfo.*            linked library versions
  main.cpp                 CLI entry point
  batch/
    BatchConverter.*       parallel conversion run (worker pool, resume, cancel)
  cache/
    CacheLayout.*          cache directory <-> file mapping
    TextureAssembler.*     header block + body -> codestream
    TextureCacheReader.*   batch iteration: open a cache, select, decode
    TextureEntries.*       texture.entries header + records
  cli/
    CliOptions.*           command line -> validated options
    Runner.*               the conversion run itself (progress, report)
  jpeg2000/
    ComponentConverter.*   component samples -> interleaved RGBA / RGB
    DecodedImage.h         decoded image (8 bit, interleaved)
    Jpeg2000Decoder.*      OpenJPEG wrapper
  png/
    PngWriter.*            8 bit RGBA/RGB PNG output
  utils/
    Constants.h            format constants (CacheFormatConfig)
    Errors.h               error hierarchy with exit codes
    FileUtils.*            binary file helpers
    Logger.*               logging facade
    UUID.*                 128 bit asset identifiers
gui/
  MainWindow.*             Qt 6 window: folders, options, progress, log, preview
  BatchWorker.*            runs sltcd::batch::run() off the GUI thread
  main.cpp                 GUI entry point + "--batch" headless mode
tests/
  data/                    frozen slice from a real cache (+ manifest.txt)
  TestCacheFixture.h       throw-away cache directory built from tests/data
  test_*.cpp               gtest suites
tools/
  make_test_slice.ps1      regenerates tests/data from a real cache
  make_reference_pixels.ps1  regenerates the reference pixels with opj_decompress
docs/
  format-notes.md          reverse engineering notes
```

## Regenerating the test slice

`tests/data/` holds ~70 KiB extracted from a real cache. Only
`entries_mini.bin` is modified (its `mEntries` field is patched so the file is
self-consistent); everything else is a verbatim copy or a decoded reference.

```powershell
pwsh -File tools/make_test_slice.ps1 -CacheDir 'D:\FS Cache\texturecache'
pwsh -File tools/make_reference_pixels.ps1
```

`reference_394.rgba` is the interleaved 16x256x4 pixel dump produced by the
standalone `opj_decompress` utility; the unit tests decode the frozen
codestream themselves and compare byte for byte with it, which pins component
order, bit depth and the inverse MCT step.

The fixture list and the expected record values are described in
`tests/data/manifest.txt`. The slice is a test vector only - no rights in it are
claimed, and it is removed on request; see [NOTICE.md](NOTICE.md).

## Legal notice

This is an independent, **educational and research** tool. It reads a texture
cache that is already on the machine it runs on, opens it read-only, decodes the
codestreams locally and never contacts the Second Life grid or any other
service; it does not circumvent any protection measure and does not grant access
to anything the user could not already read.

It is not affiliated with, authorised by, endorsed by or supported by Linden
Research, Inc. ("Second Life") or the Phoenix Firestorm Project, Inc.
("Firestorm Viewer"); those names are used descriptively, to say which file
format the tool interoperates with. Copyright in the decoded textures belongs to
their creators and/or Linden Research, Inc. - the `LICENSE` of this repository
covers the source code only, and users are responsible for having the right to
access the files they point the tool at and for complying with the Second Life
Terms of Service and with copyright law. Do not publish or redistribute decoded
textures you are not entitled to use.

The full notice, including the trademark and test data statements, is in
[NOTICE.md](NOTICE.md).

## License

MIT - see [LICENSE](LICENSE). The licence covers the original source code and
documentation of this repository; it grants no rights in any decoded content.
Third-party dependencies keep their own licences (Firestorm is GPL v2, Qt 6 is
LGPL v3 / commercial, OpenJPEG is BSD 2-clause, libpng is libpng-2.0).
