# MockCallHistoryLog

<!--introduced_in=v7.5.0-->
<!--type=module-->
<!-- source_link=lib/mock/mock-call-history.js -->

> Stability: 2 - Stable

A `MockCallHistoryLog` is an immutable snapshot of the configuration of a single
request that was intercepted by a [`MockAgent`][]. Each property mirrors a part
of the request, with the URL components already parsed by the WHATWG URL parser.

Instances are not created directly. They are produced by a [`MockCallHistory`][]
when call history is enabled on a [`MockAgent`][], and can be retrieved through
the call history helpers such as `firstCall()`, `lastCall()`, `nthCall()`,
`calls()`, and `filterCalls()`.

```mjs
import { MockAgent } from 'undici'

const mockAgent = new MockAgent({ enableCallHistory: true })

// after some requests have been intercepted:
const log = mockAgent.getCallHistory()?.firstCall()
```

## Class: `MockCallHistoryLog`

<!-- YAML
added: v7.5.0
-->

Represents the configuration of one intercepted request.

### `new MockCallHistoryLog(requestInit)`

<!-- YAML
added: v7.5.0
-->

* `requestInit` {DispatchOptions} The dispatch options of the intercepted
  request. **Default:** `{}`.

Creates a new `MockCallHistoryLog`. The `body`, `headers`, and `method`
properties are copied directly from `requestInit`, while the URL-derived
properties (`fullUrl`, `origin`, `path`, `searchParams`, `protocol`, `host`,
`port`, and `hash`) are computed from `requestInit.path` and `requestInit.origin`
using the WHATWG URL parser.

This constructor is internal-facing: instances are created by
[`MockCallHistory`][] when a request is intercepted. It is documented because the
class is part of the public API surface.

### `mockCallHistoryLog.body`

<!-- YAML
added: v7.5.0
-->

* Type: {string|null|undefined}

The body of the intercepted request, exactly as supplied to the dispatcher.

### `mockCallHistoryLog.headers`

<!-- YAML
added: v7.5.0
-->

* Type: {Record<string, string|string[]>|null|undefined}

The headers of the intercepted request.

### `mockCallHistoryLog.method`

<!-- YAML
added: v7.5.0
-->

* Type: {string}

The HTTP method of the intercepted request, for example `'GET'` or `'PUT'`.

### `mockCallHistoryLog.fullUrl`

<!-- YAML
added: v7.5.0
-->

* Type: {string}

The full URL of the intercepted request, including protocol, host, path, search
parameters, and hash.

### `mockCallHistoryLog.origin`

<!-- YAML
added: v7.5.0
-->

* Type: {string}

The origin of the intercepted request, comprising the protocol and host, for
example `'https://localhost:4000'`.

### `mockCallHistoryLog.path`

<!-- YAML
added: v7.5.0
-->

* Type: {string}

The pathname of the intercepted request. It always starts with `/` and never
contains search parameters.

### `mockCallHistoryLog.searchParams`

<!-- YAML
added: v7.5.0
-->

* Type: {Record<string, string>}

The search parameters of the intercepted request as a plain object of key/value
pairs.

### `mockCallHistoryLog.protocol`

<!-- YAML
added: v7.5.0
-->

* Type: {string}

The protocol of the intercepted request, including the trailing colon, for
example `'https:'` or `'http:'`.

### `mockCallHistoryLog.host`

<!-- YAML
added: v7.5.0
-->

* Type: {string}

The host of the intercepted request, including the port when present.

### `mockCallHistoryLog.port`

<!-- YAML
added: v7.5.0
-->

* Type: {string}

The port of the intercepted request. It is an empty string when no explicit port
was used, otherwise a string of digits.

### `mockCallHistoryLog.hash`

<!-- YAML
added: v7.5.0
-->

* Type: {string}

The hash fragment of the intercepted request. It is an empty string when absent,
otherwise a string starting with `#`.

### `mockCallHistoryLog.toMap()`

<!-- YAML
added: v7.5.0
-->

* Returns: {Map} A `Map` whose keys are the property names `protocol`, `host`,
  `port`, `origin`, `path`, `hash`, `searchParams`, `fullUrl`, `method`, `body`,
  and `headers`, each mapped to the corresponding value of this log.

Returns the log as a `Map` of property name to value.

```mjs
mockAgent.getCallHistory()?.firstCall()?.toMap().get('hash')
// '#hash'
```

### `mockCallHistoryLog.toString()`

<!-- YAML
added: v7.5.0
-->

* Returns: {string} A string representation of the log.

Returns a string built from every property name and value pair. Each pair is
joined with `->` and pairs are separated with `|`. Object values such as
`searchParams` and `headers` are serialized with `JSON.stringify()`.

```mjs
mockAgent.getCallHistory()?.firstCall()?.toString()
// protocol->https:|host->localhost:4000|port->4000|origin->https://localhost:4000|path->/endpoint|hash->#here|searchParams->{"query":"value"}|fullUrl->https://localhost:4000/endpoint?query=value#here|method->PUT|body->"{ "data": "hello" }"|headers->{"content-type":"application/json"}
```

[`MockAgent`]: MockAgent.md#class-mockagent
[`MockCallHistory`]: MockCallHistory.md#class-mockcallhistory
