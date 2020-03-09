# Notes about the `tools/icu` subdirectory

This directory contains tools and information about the
[International Components for Unicode][ICU] (ICU) integration.
Both V8 and Node.js use ICU to provide internationalization functionality.

* `patches/` are one-off patches, actually entire source file replacements,
  organized by ICU version number.
* `icu_small.json` controls the "small" (English only) ICU. It is input to
  `icutrim.py`
* `icu-generic.gyp` is the build file used for most ICU builds within ICU.
  <!-- have fun -->
* `icu-system.gyp` is an alternate build file used when `--with-intl=system-icu`
   is invoked. It builds against the `pkg-config` located ICU.
* `iculslocs.cc` is source for the `iculslocs` utility, invoked by `icutrim.py`
   as part of repackaging. Not used separately. See source for more details.
* `no-op.cc` contains an empty function to convince gyp to use a C++ compiler.
* `shrink-icu-src.py` is used during upgrade (see guide below).

Note:
> The files in this directory were written for the Node.js v0.12 effort.
> The original intent was to merge the tools such as `icutrim.py` and `iculslocs.cc`
> back into ICU. ICU has gained its own “data slicer” tool.
> There is an issue open, https://github.com/nodejs/node/issues/25136
> for replacing `icutrim.py` with the [ICU data slicer][].

## See Also

* [docs/guides/maintaining-icu.md](../../doc/guides/maintaining-icu.md) for
information on maintaining ICU in Node.js

* [docs/api/intl.md](../../doc/api/intl.md) for information on the
internationalization-related APIs in Node.js
* [The ICU Homepage][ICU]

[ICU data slicer]: https://github.com/unicode-org/icu/blob/master/docs/userguide/icu_data/buildtool.md
[ICU]: http://icu-project.org
