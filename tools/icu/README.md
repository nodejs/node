Notes about the icu directory.
===

How to upgrade ICU
---

- Make sure your node workspace is clean (clean `git status`) should be sufficient.
- Configure Node with the specific [ICU version](http://icu-project.org/download) you want to upgrade to, for example:

```
./configure \
    --with-intl=small-icu \
    --with-icu-source=http://download.icu-project.org/files/icu4c/56.1/icu4c-56_1-src.zip
make
```

(the equivalent `vcbuild.bat` commands should work also.)

- (note- may need to make changes in `icu-generic.gyp` or `tools/icu/patches` for
version specific stuff)

- Verify the node build works

```
make test-ci
```

Also running
```js
 new Intl.DateTimeFormat('es',{month:'long'}).format(new Date(9E8));
```

…Should return `January` not `enero`.
(TODO here: improve [testing](https://github.com/nodejs/Intl/issues/16))


- Now, copy `deps/icu` over to `deps/icu-small`

```
python tools/icu/shrink-icu-src.py
```

- Now, do a clean rebuild of node to test:

(TODO: fix this when these options become default)

```
./configure --with-intl=small-icu --with-icu-source=deps/icu-small
make
```

- Test this newly default-generated Node.js
```js
process.versions.icu;
new Intl.DateTimeFormat('es',{month:'long'}).format(new Date(9E8));
```

(should return your updated ICU version number, and also `January` again.)

- You are ready to check in the updated `deps/small-icu`.
This is a big commit, so make this a separate commit from other changes.

- Now, fix the default URL for the `full-icu` build in `/configure`, in
the `configure_intl()` function. It should match the ICU URL used in the
first step.  When this is done, the following should build with full ICU.

```
# clean up
rm -rf out deps/icu deps/icu4c*
./configure --with-intl=full-icu --download=all
make
make test-ci
```

- commit the change to `configure`.

-----

Notes about these tools
---

The files in this directory were written for the node.js effort. It's
the intent of their author (Steven R. Loomis / srl295) to merge them
upstream into ICU, pending much discussion within the ICU-PMC.

`icu_small.json` is somewhat node-specific as it specifies a "small ICU"
configuration file for the `icutrim.py` script. `icutrim.py` and
`iculslocs.cpp` may themselves be superseded by components built into
ICU in the future.

The following tickets were opened during this work, and their
resolution may inform the reader as to the current state of icu-trim
upstream:

   * [#10919](http://bugs.icu-project.org/trac/ticket/10919)
     (experimental branch - may copy any source patches here)
   * [#10922](http://bugs.icu-project.org/trac/ticket/10922)
     (data packaging improvements)
   * [#10923](http://bugs.icu-project.org/trac/ticket/10923)
     (rewrite data building in python)

When/if components (not including the `.json` file) are merged into
ICU, this code and `configure` will be updated to detect and use those
variants rather than the ones in this directory.
