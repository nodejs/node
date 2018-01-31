# Notes about the `tools/icu` subdirectory

This directory contains tools, data, and information about the [http://icu-project.org](ICU) (International Components for Unicode) integration. ICU is used to provide internationalization functionality.

- `patches/` are one-off patches, actually entire source file replacements, organized by ICU version number.
- `icu_small.json` controls the "small" (English only) ICU. It is input to `icutrim.py`
- `icu-generic.gyp` is the build file used for most ICU builds within ICU. <!-- have fun -->
- `icu-system.gyp` is an alternate build file used when `--with-intl=system-icu` is invoked. It builds against the `pkg-config` located ICU.
- `iculslocs.cc` is source for the `iculslocs` utility, invoked by `icutrim.py` as part of repackaging. Not used separately. See source for more details.
- `no-op.cc` — empty function to convince gyp to use a C++ compiler.
- `README.md` — you are here
- `shrink-icu-src.py` — this is used during upgrade (see guide below)

## How to upgrade ICU

- Make sure your node workspace is clean (clean `git status`) should be sufficient.
- Configure Node with the specific [ICU version](http://icu-project.org/download) you want to upgrade to, for example:

```shell
./configure \
    --with-intl=small-icu \
    --with-icu-source=http://download.icu-project.org/files/icu4c/58.1/icu4c-58_1-src.tgz
make
```

> _Note_ in theory, the equivalent `vcbuild.bat` commands should work also,
> but the commands below are makefile-centric.

- If there are ICU version-specific changes needed, you may need to make changes in `icu-generic.gyp` or add patch files to `tools/icu/patches`.
  - Specifically, look for the lists in `sources!` in the `icu-generic.gyp` for files to exclude.

- Verify the node build works:

```shell
make test-ci
```

Also running

<!-- eslint-disable strict -->

```js
new Intl.DateTimeFormat('es', {month: 'long'}).format(new Date(9E8));
```

…Should return `January` not `enero`.

- Now, copy `deps/icu` over to `deps/icu-small`

```shell
python tools/icu/shrink-icu-src.py
```

- Now, do a clean rebuild of node to test:

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

(This should print your updated ICU version number, and also `January` again.)

You are ready to check in the updated `deps/small-icu`. This is a big commit,
so make this a separate commit from the smaller changes.

- Now, rebuild the Node license.

```shell
# clean up - remove deps/icu
make clean
tools/license-builder.sh
```

- Now, fix the default URL for the `full-icu` build in `/configure`, in
the `configure_intl()` function. It should match the ICU URL used in the
first step.  When this is done, the following should build with full ICU.

```shell
# clean up
rm -rf out deps/icu deps/icu4c*
./configure --with-intl=full-icu --download=all
make
make test-ci
```

- commit the change to `configure` along with the updated `LICENSE` file.

  - Note: To simplify review, I often will “pre-land” this patch, meaning that I run the patch through `curl -L https://github.com/nodejs/node/pull/xxx.patch | git am -3 --whitespace=fix` per the collaborator’s guide… and then push that patched branch into my PR's branch. This reduces the whitespace changes that show up in the PR, since the final land will eliminate those anyway.

-----

## Postscript about the tools

The files in this directory were written for the node.js effort.
It was the intent of their author (Steven R. Loomis / srl295) to
merge them upstream into ICU, pending much discussion within the
ICU-TC.

`icu_small.json` is somewhat node-specific as it specifies a "small ICU"
configuration file for the `icutrim.py` script. `icutrim.py` and
`iculslocs.cpp` may themselves be superseded by components built into
ICU in the future. As of this writing, however, the tools are separate
entities within Node, although theyare being scrutinized by interested
members of the ICU-TC. The “upstream” ICU bugs are given below.

   * [#10919](http://bugs.icu-project.org/trac/ticket/10919)
     (experimental branch - may copy any source patches here)
   * [#10922](http://bugs.icu-project.org/trac/ticket/10922)
     (data packaging improvements)
   * [#10923](http://bugs.icu-project.org/trac/ticket/10923)
     (rewrite data building in python)
