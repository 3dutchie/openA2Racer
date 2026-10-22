# libsmacker source provenance

This directory contains libsmacker 1.2.0 source by Greg Kennedy, imported without
modification from the JonnyH mirror of the upstream SourceForge SVN repository.

- Upstream project: https://libsmacker.sourceforge.net/
- Mirror: https://github.com/JonnyH/libsmacker
- Pinned Git commit: `ae8d4c9ec07b24d43ccff184d6e512bae793dfd1`
- Upstream SVN identity reported by that commit: trunk revision 43
- Acquired archive: https://codeload.github.com/JonnyH/libsmacker/tar.gz/ae8d4c9ec07b24d43ccff184d6e512bae793dfd1
- Archive SHA-256: `02fd610fcf7d2dfe104948dffd8aee8f8e10cef1c640d4af1e5482aa52048b27`
- License: GNU Lesser General Public License version 2.1 or later, in `COPYING`
  and the `smacker.h` copyright notice.

Only `smacker.c`, `smacker.h`, and `smk_malloc.h` are compiled by OpenA2. The
upstream README, license, helper sources, and autotools build descriptions are
retained for provenance and relinking. Upstream Xcode/project IDE metadata is
omitted. OpenA2's CMake configuration builds the decoder from these sources.

The official homepage still advertises 1.1.1r35; the selected mirror identifies
itself as 1.2.0 and preserves the upstream SVN history. No upstream binary is
used, and no game movie content is present in this directory.

Native packages include `docs/source/OpenA2-0.2.0-source.zip`, containing all
OpenA2 source, this dependency source, and build scripts. Users may modify the
LGPL decoder and relink the application using those files. OpenA2 imposes no
restriction on reverse engineering for debugging such modifications.
