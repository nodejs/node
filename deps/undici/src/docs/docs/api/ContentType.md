# MIME Type Parsing

<!--introduced_in=v4.13.0-->
<!--type=module-->
<!-- source_link=lib/web/fetch/data-url.js -->

> Stability: 2 - Stable

undici exposes helpers for parsing and serializing MIME types according to the
WHATWG [MIME Sniffing Standard][]. These are the same primitives undici uses
internally to interpret `Content-Type` headers and `data:` URLs.

Import the functions from `'undici'`:

```mjs
import { parseMIMEType, serializeAMimeType } from 'undici'
```

A parsed MIME type is represented as a plain object with the following shape:

* `type` {string} The lowercased type, for example `'text'`.
* `subtype` {string} The lowercased subtype, for example `'html'`.
* `parameters` {Map<string, string>} The parameters, keyed by lowercased
  parameter name.
* `essence` {string} The concatenation of `type`, `'/'`, and `subtype`, for
  example `'text/html'`.

## `parseMIMEType(input)`

<!-- YAML
added: v4.13.0
-->

* `input` {string} The MIME type string to parse.
* Returns: {Object|string} A parsed MIME type record, or the string literal
  `'failure'` when `input` cannot be parsed.
  * `type` {string} The lowercased type.
  * `subtype` {string} The lowercased subtype.
  * `parameters` {Map<string, string>} The parsed parameters, keyed by
    lowercased parameter name.
  * `essence` {string} The MIME type essence, `` `${type}/${subtype}` ``.

Implements the WHATWG [parse a MIME type][] algorithm. The `type` and `subtype`
are lowercased, and only the first occurrence of a duplicate parameter name is
retained. When `input` is empty, lacks a `/`, or otherwise violates the grammar,
the string literal `'failure'` is returned instead of a record.

```mjs
import { parseMIMEType } from 'undici'

parseMIMEType('text/html; charset=gbk')
// {
//   type: 'text',
//   subtype: 'html',
//   parameters: Map(1) { 'charset' => 'gbk' },
//   essence: 'text/html'
// }

parseMIMEType('not a mime type')
// 'failure'
```

## `serializeAMimeType(mimeType)`

<!-- YAML
added: v5.11.0
-->

* `mimeType` {Object} A MIME type record, as returned by [`parseMIMEType()`][].
  * `type` {string} The type.
  * `subtype` {string} The subtype.
  * `parameters` {Map<string, string>} The parameters to serialize, in
    insertion order.
  * `essence` {string} The MIME type essence, `` `${type}/${subtype}` ``.
* Returns: {string} The serialized MIME type.

Implements the WHATWG [serialize a MIME type][] algorithm. The essence is
emitted followed by each parameter as `;name=value`. A parameter value that
contains characters outside the HTTP token set is quoted, and any `"` or `\`
characters within it are escaped.

```mjs
import { serializeAMimeType } from 'undici'

serializeAMimeType({
  type: 'text',
  subtype: 'html',
  parameters: new Map([['charset', 'gbk']]),
  essence: 'text/html'
})
// 'text/html;charset=gbk'
```

[MIME Sniffing Standard]: https://mimesniff.spec.whatwg.org/
[`parseMIMEType()`]: #parsemimetypeinput
[parse a MIME type]: https://mimesniff.spec.whatwg.org/#parse-a-mime-type
[serialize a MIME type]: https://mimesniff.spec.whatwg.org/#serialize-a-mime-type
