# Notes about the `tools/icu` subdirectory

This directory contains tools, data, and information about the [ICU](http://icu-project.org)
(International Components for Unicode) integration. ICU is used to provide
internationalization functionality.

- `patches/` are one-off patches, actually entire source file replacements,
  organized by ICU version number.
- `icu-generic.gyp` is the build file used for most ICU builds within ICU.
  <!-- have fun -->
- `icu-system.gyp` is an alternate build file used when `--with-intl=system-icu`
   is invoked. It builds against the `pkg-config` located ICU.
- `no-op.cc` — empty function to convince gyp to use a C++ compiler.
- `README.md` — you are here
- `shrink-icu-src.py` — this is used during upgrade (see guide below)

## How to upgrade ICU

- Make sure your Node.js workspace is clean (clean `git status`) should be
  sufficient.
- Configure Node.js with the specific [ICU version](http://icu-project.org/download)
  you want to upgrade to, for example:

```shell
./configure \
    --with-intl=full-icu \
    --with-icu-source=http://download.icu-project.org/files/icu4c/58.1/icu4c-58_1-src.tgz
make
```

> _Note_ in theory, the equivalent `vcbuild.bat` commands should work also,
> but the commands below are makefile-centric.

- If there are ICU version-specific changes needed, you may need to make changes
  in `icu-generic.gyp` or add patch files to `tools/icu/patches`.
  - Specifically, look for the lists in `sources!` in the `icu-generic.gyp` for
  files to exclude.

- Verify the Node.js build works:

```shell
make test-ci
```

Also running

<!-- eslint-disable strict -->

```js
new Intl.DateTimeFormat('es', {month: 'long'}).format(new Date(9E8));
```

…Should return `enero`.

- Now, copy `deps/icu-tmp` over to `deps/icu`

```shell
python tools/icu/shrink-icu-src.py
```

- Now, do a clean rebuild of Node.js to test:

```shell
make -k distclean
./configure
make
```

- Test this newly default-generated Node.js

<!-- eslint-disable strict -->

```js
process.versions.icu;
new Intl.DateTimeFormat('es', {month: 'long'}).format(new Date(9E8));
```

(This should print your updated ICU version number, and also `enero` again.)

You are ready to check in the updated `deps/icu`. This is a big commit,
so make this a separate commit from the smaller changes.

- Now, rebuild the Node.js license.

```shell
# clean up - remove deps/icu-tmp
make clean
tools/license-builder.sh
```

- There is no longer a URL for downloading ICU in `/configure`
because full ICU is checked in.

- Note: To simplify review, I often will “pre-land” this patch, meaning that
  I run the patch through `curl -L https://github.com/nodejs/node/pull/xxx.patch
  | git am -3 --whitespace=fix` per the collaborator’s guide… and then push that
  patched branch into my PR's branch. This reduces the whitespace changes that
  show up in the PR, since the final land will eliminate those anyway.
