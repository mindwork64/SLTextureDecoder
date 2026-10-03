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
| 9 | Batch conversion service | next |
| 10 | Robustness / error reporting | planned |
| 11 | Release build, static OpenJPEG, GUI | deferred |

The reverse engineered layout, the two populations of records and the evidence
behind them are documented in [docs/format-notes.md](docs/format-notes.md).

## Requirements

* MSYS2 `mingw-w64-x86_64` toolchain, C++20
* CMake ≥ 3.16 and Ninja
* `mingw-w64-x86_64-openjpeg2` (2.5.4), `mingw-w64-x86_64-libpng` (1.6.58)
* `mingw-w64-x86_64-gtest` (1.17.0) and `mingw-w64-x86_64-cmake`

```sh
pacman -S --needed mingw-w64-x86_64-{toolchain,cmake,ninja,openjpeg2,libpng,gtest}
```

## Build & test

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
cmake --build build
ctest --test-dir build --output-on-failure
```

Products:

* `build/bin/SLTextureDecoder.exe` – the CLI
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
texture cache therefore takes hours - use `--limit`, `--complete-only` or `--id`
while exploring.

## Layout

```
src/
  VersionInfo.*            linked library versions
  main.cpp                 CLI entry point
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
