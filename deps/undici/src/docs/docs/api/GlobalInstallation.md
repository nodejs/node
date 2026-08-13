# Global Installation

<!--introduced_in=v7.11.0-->
<!--type=module-->
<!-- source_link=index.js -->

> Stability: 2 - Stable

undici ships its own implementations of several WHATWG web APIs. The `install()`
function assigns those implementations onto `globalThis`, so they can be used
without importing them from `'undici'` first. This is useful for polyfilling
environments, guaranteeing consistent behavior across Node.js versions, and
satisfying third-party libraries that expect the relevant classes to be global.

```mjs
import { install } from 'undici'

install()

// `fetch`, `Headers`, `Response`, `Request`, etc. now resolve to undici's
// implementations.
const response = await fetch('https://example.com')
```

```cjs
const { install } = require('undici')

install()
```

## `install()`

<!-- YAML
added: v7.11.0
-->

* Returns: {undefined}

Overwrites the following properties of `globalThis` with undici's
implementations:

* `globalThis.fetch`
* `globalThis.Headers`
* `globalThis.Response`
* `globalThis.Request`
* `globalThis.FormData`
* `globalThis.WebSocket`
* `globalThis.CloseEvent`
* `globalThis.ErrorEvent`
* `globalThis.MessageEvent`
* `globalThis.EventSource`

Each installed value is the same object exported by `'undici'` under the
matching name, so the global identifiers form a single, self-consistent set.
The assignment is unconditional: any pre-existing global of the same name,
including the Node.js built-in, is replaced. The change persists for the
lifetime of the process. Calling `install()` more than once is safe and simply
re-assigns the same values.

Documentation for each installed class lives on its own page: see
[`fetch`][], [`WebSocket`][], and [`EventSource`][].

```mjs
import { install } from 'undici'

install()

const headers = new Headers([['content-type', 'application/json']])
const request = new Request('https://example.com')
const formData = new FormData()
const ws = new WebSocket('wss://example.com')
const eventSource = new EventSource('https://example.com/events')
```

### Pairing `fetch` and `FormData`

When a request body is a `FormData` instance, the `fetch` and `FormData`
implementations must come from the same source. After `install()`, both globals
resolve to undici, so they always match:

```mjs
import { install } from 'undici'

install()

const body = new FormData()
await fetch('https://example.com', { method: 'POST', body })
```

If global installation is not desired, import the matching pair directly from
`'undici'` instead:

```mjs
import { fetch, FormData } from 'undici'

const body = new FormData()
await fetch('https://example.com', { method: 'POST', body })
```

Mixing a global `FormData` with `undici.fetch()`, or `undici.FormData` with the
built-in global `fetch()`, can produce surprising multipart behavior across
Node.js and undici versions. Keep the two paired.

### Conditional installation

`install()` can be guarded so undici's `fetch` is only installed when no global
`fetch` already exists:

```mjs
import { install } from 'undici'

if (typeof globalThis.fetch === 'undefined') {
  install()
}

const response = await fetch('https://example.com')
```

[`EventSource`]: EventSource.md#class-eventsource
[`WebSocket`]: WebSocket.md#class-websocket
[`fetch`]: Fetch.md
