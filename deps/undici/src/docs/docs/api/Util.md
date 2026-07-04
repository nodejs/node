# Util

<!--introduced_in=v1.2.0-->
<!--type=module-->
<!-- source_link=lib/core/util.js -->

> Stability: 2 - Stable

A small set of utility helpers exposed for third-party implementations of the
[`Dispatcher`][] API. They cover the header-handling primitives undici uses
internally, so that custom dispatchers and handlers can normalize header names
and parse raw header lists exactly the way undici does.

```mjs
import { util } from 'undici'

const { parseHeaders, headerNameToString } = util
```

```cjs
const { util } = require('undici')

const { parseHeaders, headerNameToString } = util
```

## `parseHeaders(headers[, obj])`

<!-- YAML
added: v1.2.0
changes:
  - version: v6.1.0
    pr-url: https://github.com/nodejs/undici/pull/2501
    description: Header names supplied as a `Buffer` are accepted and normalized.
-->

* `headers` {Array} A flat list of raw header entries where even indices are
  header names and odd indices are the corresponding values. Each entry is a
  {string}, {Buffer}, or an array of {string}/{Buffer}.
* `obj` {Record<string, string|string[]>} An object to assign the parsed values
  to. When omitted, a new object is created. **Default:** `{}`.
* Returns: {Record<string, string|string[]>} The object the parsed headers were
  assigned to. When `obj` is provided, the same reference is returned.

Receives a flat list of raw header name/value pairs and returns them as an
object keyed by lowercased header name. Header names are normalized with
[`headerNameToString()`][], and `Buffer` values are decoded as `latin1`. When a
header name appears more than once, its values are collected into an array.

```mjs
import { util } from 'undici'

const raw = ['Content-Type', 'text/plain', 'Set-Cookie', 'a=1', 'Set-Cookie', 'b=2']

console.log(util.parseHeaders(raw))
// { 'content-type': 'text/plain', 'set-cookie': [ 'a=1', 'b=2' ] }
```

## `headerNameToString(value)`

<!-- YAML
added: v6.1.0
-->

* `value` {string|Buffer} The header name to normalize.
* Returns: {string} The lowercased header name.

Returns the lowercased form of a header name. The `value` may be provided as a
{string} or as a {Buffer}, in which case it is decoded as `latin1`. Common
header names are resolved through an internal lookup table for performance.

```mjs
import { util } from 'undici'

console.log(util.headerNameToString('Content-Type'))
// 'content-type'

console.log(util.headerNameToString(Buffer.from('X-Custom-Header')))
// 'x-custom-header'
```

[`Dispatcher`]: Dispatcher.md#class-dispatcher
[`headerNameToString()`]: #headernametostringvalue
