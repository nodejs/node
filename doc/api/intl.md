# Internationalization support

<!--introduced_in=v8.2.0-->

<!-- type=misc -->

Node.js has many features that make it easier to write internationalized
programs. Some of them are:

* Locale-sensitive or Unicode-aware functions in the [ECMAScript Language
  Specification][ECMA-262]:
  * [`String.prototype.normalize()`][]
  * [`String.prototype.toLowerCase()`][]
  * [`String.prototype.toUpperCase()`][]
* All functionality described in the [ECMAScript Internationalization API
  Specification][ECMA-402] (aka ECMA-402):
  * [`Intl`][] object
  * Locale-sensitive methods like [`String.prototype.localeCompare()`][] and
    [`Date.prototype.toLocaleString()`][]
* The [WHATWG URL parser][]'s [internationalized domain names][] (IDNs) support
* [`require('node:buffer').transcode()`][]
* More accurate [REPL][] line editing
* [`require('node:util').TextDecoder`][]
* [`RegExp` Unicode Property Escapes][]

Node.js and the underlying V8 engine use
[International Components for Unicode (ICU)][ICU] to implement these features
in native C/C++ code. The full ICU data set is provided by Node.js by default.
However, due to the size of the ICU data file, several
options are provided for customizing the ICU data set either when
building or running Node.js.

## Options for building Node.js

To control how ICU is used in Node.js, four `configure` options are available
during compilation. Additional details on how to compile Node.js are documented
in [BUILDING.md][].

* `--with-intl=none`/`--without-intl`
* `--with-intl=system-icu`
* `--with-intl=small-icu`
* `--with-intl=full-icu` (default)

An overview of available Node.js and JavaScript features for each `configure`
option:

| Feature                                  | `none`                            | `system-icu`                 | `small-icu`            | `full-icu` |
| ---------------------------------------- | --------------------------------- | ---------------------------- | ---------------------- | ---------- |
| [`String.prototype.normalize()`][]       | none (function is no-op)          | full                         | full                   | full       |
| `String.prototype.to*Case()`             | full                              | full                         | full                   | full       |
| [`Intl`][]                               | none (object does not exist)      | partial/full (depends on OS) | partial (English-only) | full       |
| [`String.prototype.localeCompare()`][]   | partial (not locale-aware)        | full                         | full                   | full       |
| `String.prototype.toLocale*Case()`       | partial (not locale-aware)        | full                         | full                   | full       |
| [`Number.prototype.toLocaleString()`][]  | partial (not locale-aware)        | partial/full (depends on OS) | partial (English-only) | full       |
| `Date.prototype.toLocale*String()`       | partial (not locale-aware)        | partial/full (depends on OS) | partial (English-only) | full       |
| [Legacy URL Parser][]                    | partial (no IDN support)          | full                         | full                   | full       |
| [WHATWG URL Parser][]                    | partial (no IDN support)          | full                         | full                   | full       |
| [`require('node:buffer').transcode()`][] | none (function does not exist)    | full                         | full                   | full       |
| [REPL][]                                 | partial (inaccurate line editing) | full                         | full                   | full       |
| [`require('node:util').TextDecoder`][]   | partial (basic encodings support) | partial/full (depends on OS) | partial (Unicode-only) | full       |
| [`RegExp` Unicode Property Escapes][]    | none (invalid `RegExp` error)     | full                         | full                   | full       |

The "(not locale-aware)" designation denotes that the function carries out its
operation just like the non-`Locale` version of the function, if one
exists. For example, under `none` mode, `Date.prototype.toLocaleString()`'s
operation is identical to that of `Date.prototype.toString()`.

### Disable all internationalization features (`none`)

If this option is chosen, ICU is disabled and most internationalization
features mentioned above will be **unavailable** in the resulting `node` binary.

### Build with a pre-installed ICU (`system-icu`)

Node.js can link against an ICU build already installed on the system. In fact,
most Linux distributions already come with ICU installed, and this option would
make it possible to reuse the same set of data used by other components in the
OS.

Functionalities that only require the ICU library itself, such as
[`String.prototype.normalize()`][] and the [WHATWG URL parser][], are fully
supported under `system-icu`. Features that require ICU locale data in
addition, such as [`Intl.DateTimeFormat`][] _may_ be fully or partially
supported, depending on the completeness of the ICU data installed on the
system.

### Embed a limited set of ICU data (`small-icu`)

This option makes the resulting binary link against the ICU library statically,
and includes a subset of ICU data (typically only the English locale) within
the `node` executable.

Functionalities that only require the ICU library itself, such as
[`String.prototype.normalize()`][] and the [WHATWG URL parser][], are fully
supported under `small-icu`. Features that require ICU locale data in addition,
such as [`Intl.DateTimeFormat`][], generally only work with the English locale:

```js
const january = new Date(9e8);
const english = new Intl.DateTimeFormat('en', { month: 'long' });
const spanish = new Intl.DateTimeFormat('es', { month: 'long' });

console.log(english.format(january));
// Prints "January"
console.log(spanish.format(january));
// Prints either "M01" or "January" on small-icu, depending on the user’s default locale
// Should print "enero"
```

This mode provides a balance between features and binary size.

#### Providing ICU data at runtime

If the `small-icu` option is used, one can still provide additional locale data
at runtime so that the JS methods would work for all ICU locales. Assuming the
data file is stored at `/runtime/directory/with/dat/file`, it can be made
available to ICU through either:

* The `--with-icu-default-data-dir` configure option:

  ```bash
  ./configure --with-icu-default-data-dir=/runtime/directory/with/dat/file --with-intl=small-icu
  ```

  This only embeds the default data directory path into the binary.
  The actual data file is going to be loaded at runtime from this directory
  path.

* The [`NODE_ICU_DATA`][] environment variable:

  ```bash
  env NODE_ICU_DATA=/runtime/directory/with/dat/file node
  ```

* The [`--icu-data-dir`][] CLI parameter:

  ```bash
  node --icu-data-dir=/runtime/directory/with/dat/file
  ```

When more than one of them is specified, the `--icu-data-dir` CLI parameter has
the highest precedence, then the `NODE_ICU_DATA`  environment variable, then
the `--with-icu-default-data-dir` configure option.

ICU is able to automatically find and load a variety of data formats, but the
data must be appropriate for the ICU version, and the file correctly named.
The most common name for the data file is `icudtX[bl].dat`, where `X` denotes
the intended ICU version, and `b` or `l` indicates the system's endianness.
Node.js would fail to load if the expected data file cannot be read from the
specified directory. The name of the data file corresponding to the current
Node.js version can be computed with:

```js
`icudt${process.versions.icu.split('.')[0]}${os.endianness()[0].toLowerCase()}.dat`;
```

Check ["ICU Data"][] article in the ICU User Guide for other supported formats
and more details on ICU data in general.

The [full-icu][] npm module can greatly simplify ICU data installation by
detecting the ICU version of the running `node` executable and downloading the
appropriate data file. After installing the module through `npm i full-icu`,
the data file will be available at `./node_modules/full-icu`. This path can be
then passed either to `NODE_ICU_DATA` or `--icu-data-dir` as shown above to
enable full `Intl` support.

### Embed the entire ICU (`full-icu`)

This option makes the resulting binary link against ICU statically and include
a full set of ICU data. A binary created this way has no further external
dependencies and supports all locales, but might be rather large. This is
the default behavior if no `--with-intl` flag is passed. The official binaries
are also built in this mode.

## Detecting internationalization support

To verify that ICU is enabled at all (`system-icu`, `small-icu`, or
`full-icu`), simply checking the existence of `Intl` should suffice:

```js
const hasICU = typeof Intl === 'object';
```

Alternatively, checking for `process.versions.icu`, a property defined only
when ICU is enabled, works too:

```js
const hasICU = typeof process.versions.icu === 'string';
```

To check for support for a non-English locale (i.e. `full-icu` or
`system-icu`), [`Intl.DateTimeFormat`][] can be a good distinguishing factor:

```js
const hasFullICU = (() => {
  try {
    const january = new Date(9e8);
    const spanish = new Intl.DateTimeFormat('es', { month: 'long' });
    return spanish.format(january) === 'enero';
  } catch (err) {
    return false;
  }
})();
```

For more verbose tests for `Intl` support, the following resources may be found
to be helpful:

* [btest402][]: Generally used to check whether Node.js with `Intl` support is
  built correctly.
* [Test262][]: ECMAScript's official conformance test suite includes a section
  dedicated to ECMA-402.

["ICU Data"]: http://userguide.icu-project.org/icudata
[BUILDING.md]: https://github.com/nodejs/node/blob/HEAD/BUILDING.md
[ECMA-262]: https://tc39.github.io/ecma262/
[ECMA-402]: https://tc39.github.io/ecma402/
[ICU]: http://site.icu-project.org/
[Legacy URL parser]: url.md#legacy-url-api
[REPL]: repl.md#repl
[Test262]: https://github.com/tc39/test262/tree/HEAD/test/intl402
[WHATWG URL parser]: url.md#the-whatwg-url-api
[`--icu-data-dir`]: cli.md#--icu-data-dirfile
[`Date.prototype.toLocaleString()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/toLocaleString
[`Intl.DateTimeFormat`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/DateTimeFormat
[`Intl`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl
[`NODE_ICU_DATA`]: cli.md#node_icu_datafile
[`Number.prototype.toLocaleString()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/toLocaleString
[`RegExp` Unicode Property Escapes]: https://github.com/tc39/proposal-regexp-unicode-property-escapes
[`String.prototype.localeCompare()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/localeCompare
[`String.prototype.normalize()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/normalize
[`String.prototype.toLowerCase()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/toLowerCase
[`String.prototype.toUpperCase()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/toUpperCase
[`require('node:buffer').transcode()`]: buffer.md#buffertranscodesource-fromenc-toenc
[`require('node:util').TextDecoder`]: util.md#class-utiltextdecoder
[btest402]: https://github.com/srl295/btest402
[full-icu]: https://www.npmjs.com/package/full-icu
[internationalized domain names]: https://en.wikipedia.org/wiki/Internationalized_domain_name
