# Internationalization Support

Node.js has many features that make it easier to write internationalized
programs. Some of them are:

- Locale-sensitive or Unicode-aware functions in the [ECMAScript Language
  Specification][ECMA-262]:
  - [`String.prototype.normalize()`][]
  - [`String.prototype.toLowerCase()`][]
  - [`String.prototype.toUpperCase()`][]
- All functionality described in the [ECMAScript Internationalization API
  Specification][ECMA-402] (aka ECMA-402):
  - [`Intl`][] object
  - Locale-sensitive methods like [`String.prototype.localeCompare()`][] and
    [`Date.prototype.toLocaleString()`][]
- The [WHATWG URL parser][]'s [internationalized domain names][] (IDNs) support
- [`require('buffer').transcode()`][]
- More accurate [REPL][] line editing
- [`require('util').TextDecoder`][]

Node.js (and its underlying V8 engine) uses [ICU][] to implement these features
in native C/C++ code. However, some of them require a very large ICU data file
in order to support all locales of the world. Because it is expected that most
Node.js users will make use of only a small portion of ICU functionality, only
a subset of the full ICU data set is provided by Node.js by default. Several
options are provided for customizing and expanding the ICU data set either when
building or running Node.js.

## Options for building Node.js

To control how ICU is used in Node.js, four `configure` options are available
during compilation. Additional details on how to compile Node.js are documented
in [BUILDING.md][].

- `--with-intl=none` / `--without-intl`
- `--with-intl=system-icu`
- `--with-intl=small-icu` (default)
- `--with-intl=full-icu`

An overview of available Node.js and JavaScript features for each `configure`
option:

|                                         | `none`                            | `system-icu`                 | `small-icu`            | `full-icu` |
|-----------------------------------------|-----------------------------------|------------------------------|------------------------|------------|
| [`String.prototype.normalize()`][]      | none (function is no-op)          | full                         | full                   | full       |
| `String.prototype.to*Case()`            | full                              | full                         | full                   | full       |
| [`Intl`][]                              | none (object does not exist)      | partial/full (depends on OS) | partial (English-only) | full       |
| [`String.prototype.localeCompare()`][]  | partial (not locale-aware)        | full                         | full                   | full       |
| `String.prototype.toLocale*Case()`      | partial (not locale-aware)        | full                         | full                   | full       |
| [`Number.prototype.toLocaleString()`][] | partial (not locale-aware)        | partial/full (depends on OS) | partial (English-only) | full       |
| `Date.prototype.toLocale*String()`      | partial (not locale-aware)        | partial/full (depends on OS) | partial (English-only) | full       |
| [WHATWG URL Parser][]                   | partial (no IDN support)          | full                         | full                   | full       |
| [`require('buffer').transcode()`][]     | none (function does not exist)    | full                         | full                   | full       |
| [REPL][]                                | partial (inaccurate line editing) | full                         | full                   | full       |
| [`require('util').TextDecoder`][]       | partial (basic encodings support) | partial/full (depends on OS) | partial (Unicode-only) | full       |

*Note*: The "(not locale-aware)" designation denotes that the function carries
out its operation just like the non-`Locale` version of the function, if one
exists. For example, under `none` mode, `Date.prototype.toLocaleString()`'s
operation is identical to that of `Date.prototype.toString()`.

### Disable all internationalization features (`none`)

If this option is chosen, most internationalization features mentioned above
will be **unavailable** in the resulting `node` binary.

### Build with a pre-installed ICU (`system-icu`)

Node.js can link against an ICU build already installed on the system. In fact,
most Linux distributions already come with ICU installed, and this option would
make it possible to reuse the same set of data used by other components in the
OS.

Functionalities that only require the ICU library itself, such as
[`String.prototype.normalize()`][] and the [WHATWG URL parser][], are fully
supported under `system-icu`. Features that require ICU locale data in
addition, such as [`Intl.DateTimeFormat`][] *may* be fully or partially
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
// Prints "M01" on small-icu
// Should print "enero"
```

This mode provides a good balance between features and binary size, and it is
the default behavior if no `--with-intl` flag is passed. The official binaries
are also built in this mode.

#### Providing ICU data at runtime

If the `small-icu` option is used, one can still provide additional locale data
at runtime so that the JS methods would work for all ICU locales. Assuming the
data file is stored at `/some/directory`, it can be made available to ICU
through either:

* The [`NODE_ICU_DATA`][] environmental variable:

  ```shell
  env NODE_ICU_DATA=/some/directory node
  ```

* The [`--icu-data-dir`][] CLI parameter:

  ```shell
  node --icu-data-dir=/some/directory
  ```

(If both are specified, the `--icu-data-dir` CLI parameter takes precedence.)

ICU is able to automatically find and load a variety of data formats, but the
data must be appropriate for the ICU version, and the file correctly named.
The most common name for the data file is `icudt5X[bl].dat`, where `5X` denotes
the intended ICU version, and `b` or `l` indicates the system's endianness.
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
dependencies and supports all locales, but might be rather large. See
[BUILDING.md][BUILDING.md#full-icu] on how to compile a binary using this mode.

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

- [btest402][]: Generally used to check whether Node.js with `Intl` support is
  built correctly.
- [Test262][]: ECMAScript's official conformance test suite includes a section
  dedicated to ECMA-402.

["ICU Data"]: http://userguide.icu-project.org/icudata
[`--icu-data-dir`]: cli.html#cli_icu_data_dir_file
[`Date.prototype.toLocaleString()`]: https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/Date/toLocaleString
[`Intl`]: https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/Intl
[`Intl.DateTimeFormat`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/DateTimeFormat
[`NODE_ICU_DATA`]: cli.html#cli_node_icu_data_file
[`Number.prototype.toLocaleString()`]: https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/Number/toLocaleString
[`require('buffer').transcode()`]: buffer.html#buffer_buffer_transcode_source_fromenc_toenc
[`require('util').TextDecoder`]: util.html#util_class_util_textdecoder
[`String.prototype.localeCompare()`]: https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/String/localeCompare
[`String.prototype.normalize()`]: https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/String/normalize
[`String.prototype.toLowerCase()`]: https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/String/toLowerCase
[`String.prototype.toUpperCase()`]: https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/String/toUpperCase
[BUILDING.md]: https://github.com/nodejs/node/blob/master/BUILDING.md
[BUILDING.md#full-icu]: https://github.com/nodejs/node/blob/master/BUILDING.md#build-with-full-icu-support-all-locales-supported-by-icu
[ECMA-262]: https://tc39.github.io/ecma262/
[ECMA-402]: https://tc39.github.io/ecma402/
[ICU]: http://icu-project.org/
[REPL]: repl.html#repl_repl
[Test262]: https://github.com/tc39/test262/tree/master/test/intl402
[WHATWG URL parser]: url.html#url_the_whatwg_url_api
[btest402]: https://github.com/srl295/btest402
[full-icu]: https://www.npmjs.com/package/full-icu
[internationalized domain names]: https://en.wikipedia.org/wiki/Internationalized_domain_name
