# Internationalization Support

<!--introduced_in=v8.2.0-->
<!-- type=misc -->

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
- [`RegExp` Unicode Property Escapes][]

Node.js (and its underlying V8 engine) uses [ICU][] to implement these features
in native C/C++ code. However, some of them require a very large ICU data file
in order to support all locales of the world. This is provided by Node.js by
default. Several options are provided for customizing how ICU is built and
used by Node.js.

## Options for building Node.js

To control how ICU is used in Node.js, four `configure` options are available
during compilation. Additional details on how to compile Node.js are documented
in [BUILDING.md][].

- `--with-intl=none`/`--without-intl`
- `--with-intl=system-icu`
- `--with-intl=full-icu` (default)

An overview of available Node.js and JavaScript features for each `configure`
option:

|                                         | `none`                            | `system-icu`                 | `full-icu` |
|-----------------------------------------|-----------------------------------|------------------------------|------------|
| [`String.prototype.normalize()`][]      | none (function is no-op)          | full                         | full       |
| `String.prototype.to*Case()`            | full                              | full                         | full       |
| [`Intl`][]                              | none (object does not exist)      | partial/full (depends on OS) | full       |
| [`String.prototype.localeCompare()`][]  | partial (not locale-aware)        | full                         | full       |
| `String.prototype.toLocale*Case()`      | partial (not locale-aware)        | full                         | full       |
| [`Number.prototype.toLocaleString()`][] | partial (not locale-aware)        | partial/full (depends on OS) | full       |
| `Date.prototype.toLocale*String()`      | partial (not locale-aware)        | partial/full (depends on OS) | full       |
| [WHATWG URL Parser][]                   | partial (no IDN support)          | full                         | full       |
| [`require('buffer').transcode()`][]     | none (function does not exist)    | full                         | full       |
| [REPL][]                                | partial (inaccurate line editing) | full                         | full       |
| [`require('util').TextDecoder`][]       | partial (basic encodings support) | partial/full (depends on OS) | full       |
| [`RegExp` Unicode Property Escapes][]   | none (invalid `RegExp` error)     | full                         | full       |

The "(not locale-aware)" designation denotes that the function carries out its
operation just like the non-`Locale` version of the function, if one
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

#### Providing ICU data at runtime

If the `full-icu` option is used, one can still provide additional data
at runtime. (This data does not override the built-in data.) Assuming the
data file is stored at `/some/directory`, it can be made available to ICU
through either:

* The [`NODE_ICU_DATA`][] environment variable:

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

### Embed the entire ICU (`full-icu`)

This option makes the resulting binary link against ICU statically and include
a full set of ICU data. A binary created this way has no further external
dependencies and supports all locales. This is the default.

### `small-icu`

Support for `small-icu` (English only) has been removed. See
["ICU Data"][] for information on how to customize your ICU build.

## Detecting internationalization support

To verify that ICU is enabled at all (`system-icu` or
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
[`Date.prototype.toLocaleString()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/toLocaleString
[`Intl`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl
[`Intl.DateTimeFormat`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/DateTimeFormat
[`NODE_ICU_DATA`]: cli.html#cli_node_icu_data_file
[`Number.prototype.toLocaleString()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/toLocaleString
[`RegExp` Unicode Property Escapes]: https://github.com/tc39/proposal-regexp-unicode-property-escapes
[`require('buffer').transcode()`]: buffer.html#buffer_buffer_transcode_source_fromenc_toenc
[`require('util').TextDecoder`]: util.html#util_class_util_textdecoder
[`String.prototype.localeCompare()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/localeCompare
[`String.prototype.normalize()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/normalize
[`String.prototype.toLowerCase()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/toLowerCase
[`String.prototype.toUpperCase()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/toUpperCase
[BUILDING.md]: https://github.com/nodejs/node/blob/master/BUILDING.md
[ECMA-262]: https://tc39.github.io/ecma262/
[ECMA-402]: https://tc39.github.io/ecma402/
[ICU]: http://icu-project.org/
[REPL]: repl.html#repl_repl
[Test262]: https://github.com/tc39/test262/tree/master/test/intl402
[WHATWG URL parser]: url.html#url_the_whatwg_url_api
[btest402]: https://github.com/srl295/btest402
[internationalized domain names]: https://en.wikipedia.org/wiki/Internationalized_domain_name
