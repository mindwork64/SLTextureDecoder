# SLTextureDecoder

Decodes Second Life / Firestorm JPEG 2000 texture caches
(`texture.entries` + `texture.cache` + per-texture `.texture` files) into PNG.

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
| 11 | Robustness / error reporting | planned |
| 12 | Release build, static OpenJPEG | deferred |

The reverse engineered layout, the two populations of records and the evidence
behind them are documented in [docs/format-notes.md](docs/format-notes.md).

## Requirements

* MSYS2 `mingw-w64-x86_64` toolchain, C++20
* CMake ≥ 3.16 and Ninja
* `mingw-w64-x86_64-openjpeg2` (2.5.4), `mingw-w64-x86_64-libpng` (1.6.58)
* `mingw-w64-x86_64-gtest` (1.17.0) and `mingw-w64-x86_64-cmake`
* optional, for the GUI: `mingw-w64-x86_64-qt6-base`

```sh
pacman -S --needed mingw-w64-x86_64-{toolchain,cmake,ninja,openjpeg2,libpng,gtest,qt6-base}
```

Without Qt 6 the GUI target is skipped silently; `-DSLTCD_BUILD_GUI=OFF` turns it
off explicitly.

## Build & test

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
cmake --build build
ctest --test-dir build --output-on-failure
```

Products:

* `build/bin/SLTextureDecoder.exe` – the CLI
* `build/bin/SLTextureDecoderGUI.exe` – the Qt 6 desktop frontend (needs the
  `C:\msys64\mingw64\bin` directory, or `windeployqt`, on `PATH`)
* `build/bin/sl_texture_decoder_tests.exe` – the unit tests
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
| `-v`, `--verbose` | log every record |
| `-h`, `--help`, `-V`, `--version` | help / version |

Each texture becomes `<uuid>.png` (complete codestream) or `<uuid>.partial.png`
(best effort from a truncated one - see
[docs/format-notes.md](docs/format-notes.md)). The exit code is `0` when every
selected texture was converted, otherwise the code of the first failure
(`3` entry not found, `6` decode failure, ... - see `utils/Errors.h`).

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
  to use more than one core; on a 2 core / 4 thread CPU a `--jobs 4` run is
  about 2.3x faster than a single threaded one, with byte identical output.
* **Overwrite existing files** stays off by default, so the PNGs already in the
  output folder are skipped: an interrupted run is resumed by starting it again.
* The last used folders and options are remembered in
  `HKCU\Software\SLTextureDecoder\GUI`.
* Browse the results in Explorer with **Open output folder**.

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
    ComponentConverter.*   component samples -> interleaved RGBA
    DecodedImage.h         decoded image (8 bit, interleaved)
    Jpeg2000Decoder.*      OpenJPEG wrapper
  png/
    PngWriter.*            8 bit RGBA PNG output
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
`tests/data/manifest.txt`.
