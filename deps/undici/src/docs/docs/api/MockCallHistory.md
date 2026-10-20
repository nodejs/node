# MockCallHistory

<!--introduced_in=v7.5.0-->
<!--type=module-->
<!-- source_link=lib/mock/mock-call-history.js -->

> Stability: 2 - Stable

A `MockCallHistory` records the configuration of every request intercepted by a
[`MockAgent`][] for which call history is enabled. Each recorded request is
stored as a [`MockCallHistoryLog`][], and the `MockCallHistory` exposes helpers
to read and filter those logs in tests.

Instances are not created directly. Enable call history on a [`MockAgent`][] and
retrieve the history with `mockAgent.getCallHistory()`:

```mjs
import { MockAgent } from 'undici'

const mockAgent = new MockAgent({ enableCallHistory: true })
const callHistory = mockAgent.getCallHistory()
```

Call history can also be enabled after construction:

```mjs
import { MockAgent } from 'undici'

const mockAgent = new MockAgent()
mockAgent.enableCallHistory()
const callHistory = mockAgent.getCallHistory()
```

## Class: `MockCallHistory`

<!-- YAML
added: v7.5.0
-->

Tracks the configuration of intercepted requests as an ordered collection of
[`MockCallHistoryLog`][] entries.

A `MockCallHistory` is iterable. Use it with a `for...of` loop, the spread
operator, or any constructor that accepts an iterable:

```mjs
for (const log of mockAgent.getCallHistory()) {
  // ...
}

const logs = [...mockAgent.getCallHistory()]
const logSet = new Set(mockAgent.getCallHistory())
```

### `mockCallHistory.calls()`

<!-- YAML
added: v7.5.0
-->

* Returns: {MockCallHistoryLog[]} All recorded logs, in the order the requests
  were intercepted.

Returns every recorded [`MockCallHistoryLog`][] as an array.

```mjs
mockAgent.getCallHistory()?.calls()
```

### `mockCallHistory.firstCall()`

<!-- YAML
added: v7.5.0
-->

* Returns: {MockCallHistoryLog|undefined} The first recorded log, or `undefined`
  when no request has been recorded.

Returns the first recorded [`MockCallHistoryLog`][].

```mjs
mockAgent.getCallHistory()?.firstCall()
```

### `mockCallHistory.lastCall()`

<!-- YAML
added: v7.5.0
-->

* Returns: {MockCallHistoryLog|undefined} The last recorded log, or `undefined`
  when no request has been recorded.

Returns the last recorded [`MockCallHistoryLog`][].

```mjs
mockAgent.getCallHistory()?.lastCall()
```

### `mockCallHistory.nthCall(position)`

<!-- YAML
added: v7.5.0
-->

* `position` {number} The 1-based position of the log to return. Must be a
  positive integer.
* Returns: {MockCallHistoryLog|undefined} The log at `position`, or `undefined`
  when no log exists at that position.

Returns the [`MockCallHistoryLog`][] at the given 1-based `position`. The index
is 1-based for readability, so `nthCall(1)` is equivalent to `firstCall()`.

Throws an [`InvalidArgumentError`][] when `position` is not a number, not an
integer, or not positive. Use `firstCall()` or `lastCall()` instead of passing a
non-positive value.

```mjs
mockAgent.getCallHistory()?.nthCall(3) // the third recorded log
```

### `mockCallHistory.filterCalls(criteria[, options])`

<!-- YAML
added: v7.5.0
-->

* `criteria` {Function|RegExp|Object} The filter to apply.
  * When a {Function} is given, it is used as the predicate of
    `Array.prototype.filter` and receives each [`MockCallHistoryLog`][]; logs for
    which it returns a truthy value are kept.
  * When a {RegExp} is given, a log is kept when the expression matches its
    [`mockCallHistoryLog.toString()`][] representation.
  * When an {Object} is given, each property is a request field to filter on and
    each value is a [filter parameter](#filter-parameter). An empty object
    returns every log.
    * `protocol` {string|RegExp|null|undefined} Filters by request protocol.
    * `host` {string|RegExp|null|undefined} Filters by request host.
    * `port` {string|RegExp|null|undefined} Filters by request port.
    * `origin` {string|RegExp|null|undefined} Filters by request origin.
    * `path` {string|RegExp|null|undefined} Filters by request path.
    * `hash` {string|RegExp|null|undefined} Filters by request hash.
    * `fullUrl` {string|RegExp|null|undefined} Filters by request full URL.
    * `method` {string|RegExp|null|undefined} Filters by request method.
* `options` {Object} Adjusts the filtering behavior. Only applied when `criteria`
  is an object. (optional)
  * `operator` {string} How to combine multiple object properties. `'OR'` keeps
    a log matching any criterion; `'AND'` keeps a log matching every criterion.
    The value is case-insensitive. **Default:** `'OR'`.
* Returns: {MockCallHistoryLog[]} The matching logs. Duplicates produced by an
  `'OR'` combination are removed.

A convenience method for applying one or several filters at once. It is a more
expressive alternative to the per-field helpers below.

Throws an [`InvalidArgumentError`][] when `criteria` is not a function, regular
expression, or object, or when `options.operator` is neither `'OR'` nor `'AND'`.

```mjs
const callHistory = mockAgent.getCallHistory()

callHistory?.filterCalls((log) =>
  log.hash === '#hash' && log.headers?.authorization !== undefined)

callHistory?.filterCalls(/"errors": "wrong body"/)

// Logs with a hash containing `my-hash` OR a path equal to `/endpoint`.
callHistory?.filterCalls({ hash: /my-hash/, path: '/endpoint' })

// Logs with a hash containing `my-hash` AND a path equal to `/endpoint`.
callHistory?.filterCalls(
  { hash: /my-hash/, path: '/endpoint' },
  { operator: 'AND' }
)
```

### `mockCallHistory.filterCallsByProtocol(criteria)`

<!-- YAML
added: v7.5.0
-->

* `criteria` {string|RegExp|null|undefined} See
  [filter parameter](#filter-parameter).
* Returns: {MockCallHistoryLog[]} The logs whose protocol matches `criteria`.

Filters the recorded logs by request protocol.

```mjs
mockAgent.getCallHistory()?.filterCallsByProtocol(/https/)
mockAgent.getCallHistory()?.filterCallsByProtocol('https:')
```

### `mockCallHistory.filterCallsByHost(criteria)`

<!-- YAML
added: v7.5.0
-->

* `criteria` {string|RegExp|null|undefined} See
  [filter parameter](#filter-parameter).
* Returns: {MockCallHistoryLog[]} The logs whose host matches `criteria`.

Filters the recorded logs by request host.

```mjs
mockAgent.getCallHistory()?.filterCallsByHost(/localhost/)
mockAgent.getCallHistory()?.filterCallsByHost('localhost:3000')
```

### `mockCallHistory.filterCallsByPort(criteria)`

<!-- YAML
added: v7.5.0
-->

* `criteria` {string|RegExp|null|undefined} See
  [filter parameter](#filter-parameter).
* Returns: {MockCallHistoryLog[]} The logs whose port matches `criteria`.

Filters the recorded logs by request port.

```mjs
mockAgent.getCallHistory()?.filterCallsByPort(/3000/)
mockAgent.getCallHistory()?.filterCallsByPort('3000')
mockAgent.getCallHistory()?.filterCallsByPort('')
```

### `mockCallHistory.filterCallsByOrigin(criteria)`

<!-- YAML
added: v7.5.0
-->

* `criteria` {string|RegExp|null|undefined} See
  [filter parameter](#filter-parameter).
* Returns: {MockCallHistoryLog[]} The logs whose origin matches `criteria`.

Filters the recorded logs by request origin.

```mjs
mockAgent.getCallHistory()?.filterCallsByOrigin(/http:\/\/localhost:3000/)
mockAgent.getCallHistory()?.filterCallsByOrigin('http://localhost:3000')
```

### `mockCallHistory.filterCallsByPath(criteria)`

<!-- YAML
added: v7.5.0
-->

* `criteria` {string|RegExp|null|undefined} See
  [filter parameter](#filter-parameter).
* Returns: {MockCallHistoryLog[]} The logs whose path matches `criteria`.

Filters the recorded logs by request path.

```mjs
mockAgent.getCallHistory()?.filterCallsByPath(/api\/v1\/graphql/)
mockAgent.getCallHistory()?.filterCallsByPath('/api/v1/graphql')
```

### `mockCallHistory.filterCallsByHash(criteria)`

<!-- YAML
added: v7.5.0
-->

* `criteria` {string|RegExp|null|undefined} See
  [filter parameter](#filter-parameter).
* Returns: {MockCallHistoryLog[]} The logs whose hash matches `criteria`.

Filters the recorded logs by request hash.

```mjs
mockAgent.getCallHistory()?.filterCallsByHash(/hash/)
mockAgent.getCallHistory()?.filterCallsByHash('#hash')
```

### `mockCallHistory.filterCallsByFullUrl(criteria)`

<!-- YAML
added: v7.5.0
-->

* `criteria` {string|RegExp|null|undefined} See
  [filter parameter](#filter-parameter).
* Returns: {MockCallHistoryLog[]} The logs whose full URL matches `criteria`.

Filters the recorded logs by request full URL. The full URL contains the
protocol, host, port, path, query parameters, and hash.

```mjs
mockAgent.getCallHistory()?.filterCallsByFullUrl(/https:\/\/localhost:3000\/\?query=value#hash/)
mockAgent.getCallHistory()?.filterCallsByFullUrl('https://localhost:3000/?query=value#hash')
```

### `mockCallHistory.filterCallsByMethod(criteria)`

<!-- YAML
added: v7.5.0
-->

* `criteria` {string|RegExp|null|undefined} See
  [filter parameter](#filter-parameter).
* Returns: {MockCallHistoryLog[]} The logs whose method matches `criteria`.

Filters the recorded logs by request method.

```mjs
mockAgent.getCallHistory()?.filterCallsByMethod(/POST/)
mockAgent.getCallHistory()?.filterCallsByMethod('POST')
```

### `mockCallHistory.clear()`

<!-- YAML
added: v7.5.0
-->

* Returns: {undefined}

Removes every recorded [`MockCallHistoryLog`][]. This is performed
automatically when `mockAgent.close()` is called.

```mjs
mockAgent.clearCallHistory()
// Equivalent to:
mockAgent.getCallHistory()?.clear()
```

### `mockCallHistory[Symbol.iterator]()`

<!-- YAML
added: v7.5.0
-->

* Returns: {Generator} A generator yielding each recorded
  [`MockCallHistoryLog`][] in order.

Makes a `MockCallHistory` iterable, yielding the same logs returned by
`calls()`. This enables `for...of` iteration, the spread operator, and
constructing other iterables from a history.

```mjs
const callHistory = mockAgent.getCallHistory()

for (const log of callHistory) {
  console.log(log.method, log.fullUrl)
}

const logs = [...callHistory]
```

## Filter parameter

The per-field `filterCallsBy*` helpers and the object form of `filterCalls()`
accept a filter parameter that is matched against the corresponding field of each
[`MockCallHistoryLog`][]:

* A {string} keeps a log only when the field is strictly equal to the value.
* `null` keeps a log only when the field is strictly equal to `null`.
* `undefined` keeps a log only when the field is strictly equal to `undefined`.
* A {RegExp} keeps a log only when the expression matches the field.

Any other value throws an [`InvalidArgumentError`][].

[`InvalidArgumentError`]: Errors.md#class-invalidargumenterror
[`MockAgent`]: MockAgent.md#class-mockagent
[`MockCallHistoryLog`]: MockCallHistoryLog.md#class-mockcallhistorylog
[`mockCallHistoryLog.toString()`]: MockCallHistoryLog.md#mockcallhistorylogtostring
