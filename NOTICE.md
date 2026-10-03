# Legal notice

SLTextureDecoder is an **educational and research tool** for people who want to
understand how a Second Life / Firestorm texture cache is laid out on disk and
how JPEG 2000 codestreams are reassembled from it. It is **not affiliated with,
authorised by, endorsed by, sponsored by or supported by** Linden Research, Inc.
("Linden Lab", "Second Life") or the Phoenix Firestorm Project, Inc.
("Firestorm Viewer"), and it is not a Second Life client of any kind.

## What the software does

* It reads a texture cache directory that is **already present on the machine
  where it is started** - the files the locally installed viewer wrote for the
  account of the person running it (`texture.entries`, `texture.cache` and the
  `0..f/<uuid>.texture` shards).
* It reassembles the codestream bytes from those two/three files, decodes them
  locally with OpenJPEG and writes the result as a PNG file of the user's own
  choosing.
* It never connects to the Second Life grid, never authenticates anywhere, never
  downloads assets, never reads another user's cache, and never modifies or
  deletes the cache it reads (the cache is opened read-only).

## What the software does not do

* It is not a viewer, a bot, a scraper or a network tool, and it does not
  interact with any Linden Lab or Firestorm service.
* It does not break, bypass or remove any protection measure. The cache is not
  encrypted and is not protected against reading; the tool only joins two parts
  of a file the viewer itself wrote for the same user, and it does not grant
  access to anything that was not already accessible to that user.
* It does not include, redistribute or reconstruct the Second Life client, the
  Firestorm viewer or any of their source code.

## Rights in the decoded content

The PNG files this tool writes are copies of textures that the user's own viewer
downloaded while they used Second Life. Copyright and all other rights in those
textures and in any other in-world content belong to their respective creators
and/or Linden Research, Inc.; this project claims no rights in them, and the
`LICENSE` of this repository does not apply to them.

Reading the cache and keeping decoded copies of it on your own machine is a
matter between you, the Second Life Terms of Service
(https://www.lindenlab.com/legal and
https://wiki.secondlife.com/wiki/Linden_Lab_Official:Terms_of_Service),
the licence of the viewer you use (the Firestorm viewer is distributed under the
GNU GPL v2) and copyright law (in the United States, the DMCA, 17 U.S.C. § 512
and § 1201 ff.). **Users are solely responsible** for having the right to access
the files they point this tool at, and must not use it to obtain, copy,
publish, sell or otherwise exploit textures or other content they are not
entitled to use, nor to circumvent any technical protection measure.

In particular: do not redistribute decoded textures, do not use the tool against
a cache that is not yours, and do not use it in any way that infringes the
rights of others or violates the applicable terms of service.

## Trademarks

"Second Life" and "Linden Lab" are trademarks of Linden Research, Inc.
"Firestorm" is a trademark of the Phoenix Firestorm Project, Inc. "Qt" is a
trademark of The Qt Company Ltd. "OpenJPEG" and "Portable Network Graphics"
refer to projects of their respective owners. All names are used here
**descriptively only**, to state which file format and which software this
independent tool interoperates with; no affiliation, sponsorship or endorsement
is implied in either direction.

## Test data in this repository

`tests/data/` contains a ~70 KiB slice extracted from a cache (a few record
headers, two small `.texture` bodies and a decoded 16x256 pixel reference dump).
It exists solely as a test vector, so that the unit tests can verify the
reassembly and the component order without access to a live cache. No rights in
that data are claimed; the slices are the minimum needed for the tests. A
rightsholder who objects to any file in this repository can open an issue and it
will be removed or replaced immediately.

## Licence

The original source code and documentation of this repository are released
under the MIT licence - see `LICENSE`. That licence covers the source code only:
it grants **no rights** in any texture, image, model or other content the tool
may read, decode, convert or write, nor in the test data described above. Such
content remains the property of its respective rightsholders and is governed by
their own terms, by the Second Life Terms of Service and by applicable copyright
law.

## No warranty

The software is provided "as is", without warranty of any kind - see `LICENSE`.
The authors are not liable for any use of the software or of its output,
including any claim by a third party arising from that use.
