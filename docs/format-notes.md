# Texture cache format notes

Everything below was verified against a real cache (`D:\FS Cache\texturecache`,
Firestorm) and cross-checked against the Firestorm sources
(`indra/newview/lltexturecache.{h,cpp}`). Numbers in *italics* are values of
that concrete cache.

## Files

| File | Layout | Size in the reference cache |
| --- | --- | --- |
| `texture.entries` | 44 byte header + N × 28 byte records | 5 507 644 = 44 + 196 700 × 28 |
| `texture.cache` | N × 600 bytes: the first 600 bytes of every texture, **in the same order as the records** | 118 020 000 = 196 700 × 600 |
| `FastCache.cache` | N × 1040 bytes: 16 bytes of `(w, h, c, discardLevel)` as `S32` followed by a ≤16×16 RGBA preview | 204 568 000 = 196 700 × 1040 |
| `<0..f>/<uuid>.texture` | remainder of the codestream of one texture | 54 825 files, ≈7.3 GiB |

The shard directory is the **first hexadecimal digit of the UUID**, i.e. the
high nibble of byte 0 (`87d1503c-…` → `8/`, `e569711a-…` → `e/`).

`texture.cache` and `texture.entries` are preallocated to the configured maximum
number of records (`sCacheMaxEntries = 196 700` here), so both files contain
exactly `entriesCount` slots even though many of them are free.

## `texture.entries` header (44 bytes, little endian)

```
F32  mVersion           1.71          ("1.71" in the reference cache)
U32  mAdressSize        64            (note: misspelled in the viewer source)
char mEncoderVersion[32] "KDU v8.4.1" (NUL padded)
S32  mEntries           196700        (capacity, not the number of live records)
```

## `texture.entries` record (28 bytes, little endian)

```
LLUUID mID         16 bytes, canonical big-endian order (verified: the bytes
                   match the <uuid>.texture file name one to one)
S32    mImageSize  "total size of image if known"; -1 for a brand new record
S32    mBodySize   size of the <uuid>.texture body file; <= 0 means "free slot"
U32    mTime       seconds since 1970-01-01
```

Verified invariants:

* The number of records with `mBodySize > 0` equals the number of `.texture`
  files on disk exactly (*54 825*).
* A free slot may still have its old `.texture` file on disk (record 0 of the
  reference cache: `mBodySize = 0`, file present, 31 875 bytes).
* `mTime` is *not* monotonically increasing: records are written into reused
  slots, and 62 189 of 196 700 records break the monotonic order. Do not assume
  any ordering of the array.

## How a codestream is assembled

```
codestream = texture.cache[index * 600 .. index * 600 + 600] ++ <uuid>.texture
```

This matches the writer in `LLTextureCacheWorker::doWrite()`: the header block
holds `mWriteData[0..600]`, the body file holds `mWriteData[600..mDataSize]`
and `mBodySize = mDataSize - 600`.

## Two populations of records

Splitting the 54 825 live records by `mImageSize - mBodySize` yields exactly two
groups:

| Group | Count | `mImageSize - mBodySize` | COM marker in the header block | Decodes |
| --- | --- | --- | --- | --- |
| A – complete | 22 790 | 600 | `Kakadu-v4.2.1`, `Kdu-Layer-Info: …` | yes (12/12 sampled) |
| B – partial | 32 033 | 601 | `\x00\x01a=<uuid>&c=<rgba>&z=<yyyyMMddHHmmss>` | no (0/6 sampled) |

### Group A is a complete codestream

* `PSOT` of the `SOT` marker equals `600 + mBodySize - sotOffset - 2`, i.e. the
  assembled stream length, and the stream ends with the `EOC` marker (`FF D9`).
* `bpp = 8 * streamSize / (w * h * components)` is a realistic value
  (e.g. 1.89 bpp for a 64×64×4 texture).
* `opj_decompress` decodes the assembled stream into a PNG without warnings.

### Group B stores only a prefix of the codestream

* `PSOT` describes a *much* larger codestream (e.g. 282 471 for a 1024×1024×3
  texture ⇒ 0.719 bpp, a plausible full-quality size) while the cached data
  would be 0.016 bpp – physically impossible for a real image.
* The assembled stream does **not** end with `EOC`.
* `opj_decompress` fails with `Tile part length size inconsistent with stream
  length`. Patching `PSOT` to the length of the cached data reveals the real
  problem: `segment too long (78) with max (35) for codeblock …` – the last
  packet is cut in the middle, i.e. the data is truncated.
* The record itself is self-consistent (`mImageSize == mBodySize + 601`) and the
  body file size always matches `mBodySize`, so the *record* and the *file* are
  intact; only the codestream bytes beyond the cached prefix are missing.

Practical consequence: group B entries cannot be decoded to a complete image
from the cache alone. The tool must classify them
(`TextureEntry::isComplete()` / `isPartial()`) and report them instead of
producing silently broken PNG files.

## Open questions

* What produced group B? The `a=<uuid>&c=<rgba>&z=<timestamp>` COM payload is
  sim/bake metadata and the `a=` UUIDs are **not** present in `texture.entries`
  (they are source asset ids of derived textures). Whatever the cause, the
  viewer cached fewer bytes than its own record announces.
* Is group B recoverable with a *lenient* OpenJPEG decode
  (`opj_decoder_set_strict_mode(OPJ_FALSE)`)? To be tested when the decoder
  stage is implemented; a partially decoded image would still be better than
  nothing.
* `FastCache.cache` is not needed for decoding. It is documented here only
  because it shares the record index space.
