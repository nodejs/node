# EventSource

<!--introduced_in=v6.5.0-->
<!--type=module-->
<!-- source_link=lib/web/eventsource/eventsource.js -->

> Stability: 1 - Experimental

`EventSource` is a [WHATWG-conformant][] implementation of the
[server-sent events][] interface. It opens a persistent HTTP connection to a
server that responds with the `text/event-stream` content type and dispatches
the events it receives without closing the connection.

```mjs
import { EventSource } from 'undici'

const eventSource = new EventSource('http://localhost:3000')
eventSource.onmessage = (event) => {
  console.log(event.data)
}
```

Constructing an `EventSource` for the first time emits a one-time
`ExperimentalWarning` with the code `'UNDICI-ES'`. The interface is also
installed onto `globalThis` as `globalThis.EventSource`.

## Class: `EventSource`

<!-- YAML
added: v6.5.0
-->

> Stability: 1 - Experimental

* Extends: {EventTarget}

The `EventSource` interface receives server-sent events over an HTTP
connection. Instances are created with the [`new EventSource()`](#new-eventsourceurl-eventsourceinitdict)
constructor and emit [`'open'`](#event-open), [`'message'`](#event-message), and
[`'error'`](#event-error) events.

### `new EventSource(url[, eventSourceInitDict])`

<!-- YAML
added: v6.5.0
-->

> Stability: 1 - Experimental

* `url` {string|URL} The URL of the event stream. Relative URLs are resolved
  against the environment's base URL.
* `eventSourceInitDict` {Object} (optional)
  * `withCredentials` {boolean} When `true`, the request is made with the
    credentials mode set to `include` and CORS attribute state set to
    `use-credentials`; otherwise the credentials mode is `same-origin`.
    **Default:** `false`.
  * `dispatcher` {Dispatcher} The dispatcher used for the underlying request.
    **Default:** the global dispatcher.

    > Stability: 0 - Deprecated. Use `node.dispatcher` instead.
  * `node` {Object} undici-specific extensions to the standard
    `EventSourceInit` dictionary.
    * `dispatcher` {Dispatcher} The dispatcher used for the underlying request.
      **Default:** the global dispatcher.
    * `reconnectionTime` {number} The reconnection time, in milliseconds, to
      wait before re-establishing a dropped connection. The server may override
      this value with a `retry` field. **Default:** `3000`.

Creates a new `EventSource` and immediately begins connecting to `url`. The
request is sent with the `Accept: text/event-stream` header, a cache mode of
`no-store`, and an initiator type of `other`.

If `url` cannot be parsed, a `SyntaxError` `DOMException` is thrown.

```mjs
import { EventSource } from 'undici'

const eventSource = new EventSource('http://localhost:3000', {
  withCredentials: true
})
```

A custom [`Dispatcher`][] can be supplied to control the underlying request, for
example to add request headers:

```mjs
import { EventSource, Agent } from 'undici'

class CustomHeaderAgent extends Agent {
  dispatch (opts) {
    opts.headers['x-custom-header'] = 'hello world'
    return super.dispatch(...arguments)
  }
}

const eventSource = new EventSource('http://localhost:3000', {
  node: {
    dispatcher: new CustomHeaderAgent()
  }
})
```

### `eventSource.close()`

<!-- YAML
added: v6.5.0
-->

> Stability: 1 - Experimental

* Returns: {undefined}

Closes the connection, if any, aborts the underlying request, and sets
[`eventSource.readyState`](#eventsourcereadystate) to `CLOSED`. Once closed, the
`EventSource` does not attempt to reconnect. Calling `close()` on an already
closed `EventSource` has no effect.

### `eventSource.readyState`

<!-- YAML
added: v6.5.0
-->

> Stability: 1 - Experimental

* Type: {number}

A read-only number representing the state of the connection. It is one of the
following constants:

* [`EventSource.CONNECTING`](#eventsourceconnecting) (`0`) — the connection has
  not yet been established, or it was closed and is being re-established.
* [`EventSource.OPEN`](#eventsourceopen) (`1`) — the connection is open and
  events are being dispatched as they are received.
* [`EventSource.CLOSED`](#eventsourceclosed) (`2`) — the connection is not open
  and is not being re-established.

### `eventSource.url`

<!-- YAML
added: v6.5.0
-->

> Stability: 1 - Experimental

* Type: {string}

A read-only string providing the URL of the event stream, after resolution
against the environment's base URL.

### `eventSource.withCredentials`

<!-- YAML
added: v6.5.0
-->

> Stability: 1 - Experimental

* Type: {boolean}

A read-only boolean indicating whether the `EventSource` was instantiated with
CORS credentials (`true`), or not (`false`, the default). It reflects the
`withCredentials` option passed to the constructor.

### `eventSource.onopen`

<!-- YAML
added: v6.5.0
-->

> Stability: 1 - Experimental

* Type: {Function|null} **Default:** `null`.

An event handler that is invoked when an [`'open'`](#event-open) event is
dispatched. Assigning a function registers it as the handler; assigning `null`
removes the current handler.

### `eventSource.onmessage`

<!-- YAML
added: v6.5.0
-->

> Stability: 1 - Experimental

* Type: {Function|null} **Default:** `null`.

An event handler that is invoked when a [`'message'`](#event-message) event is
dispatched. Assigning a function registers it as the handler; assigning `null`
removes the current handler.

### `eventSource.onerror`

<!-- YAML
added: v6.5.0
-->

> Stability: 1 - Experimental

* Type: {Function|null} **Default:** `null`.

An event handler that is invoked when an [`'error'`](#event-error) event is
dispatched. Assigning a function registers it as the handler; assigning `null`
removes the current handler.

### `EventSource.CONNECTING`

<!-- YAML
added: v6.5.0
-->

> Stability: 1 - Experimental

* Type: {number}

The numeric constant `0`, representing the `CONNECTING`
[`readyState`](#eventsourcereadystate). It is defined as a read-only,
non-writable property on both the `EventSource` constructor and instances.

### `EventSource.OPEN`

<!-- YAML
added: v6.5.0
-->

> Stability: 1 - Experimental

* Type: {number}

The numeric constant `1`, representing the `OPEN`
[`readyState`](#eventsourcereadystate). It is defined as a read-only,
non-writable property on both the `EventSource` constructor and instances.

### `EventSource.CLOSED`

<!-- YAML
added: v6.5.0
-->

> Stability: 1 - Experimental

* Type: {number}

The numeric constant `2`, representing the `CLOSED`
[`readyState`](#eventsourcereadystate). It is defined as a read-only,
non-writable property on both the `EventSource` constructor and instances.

### Event: `'open'`

<!-- YAML
added: v6.5.0
-->

> Stability: 1 - Experimental

Emitted when the connection is established and the
[`readyState`](#eventsourcereadystate) becomes `OPEN`. The listener receives an
{Event}.

```mjs
import { EventSource } from 'undici'

const eventSource = new EventSource('http://localhost:3000')
eventSource.addEventListener('open', () => {
  console.log('connection opened')
})
```

### Event: `'message'`

<!-- YAML
added: v6.15.0
-->

> Stability: 1 - Experimental

Emitted when a message that has no explicit `event` field is received. The
listener receives a {MessageEvent} whose `data`, `lastEventId`, and `origin`
properties are populated from the server-sent event. Named events (those with an
`event` field) are dispatched under their own type and must be subscribed to with
[`addEventListener()`][].

```mjs
import { createServer } from 'node:http'
import { EventSource } from 'undici'

const server = createServer((request, response) => {
  response.writeHead(200, {
    'content-type': 'text/event-stream',
    'cache-control': 'no-cache',
    connection: 'keep-alive'
  })

  response.write('event: ping\n')
  response.write('data: connected\n\n')

  const interval = setInterval(() => {
    response.write(`data: ${Date.now()}\n\n`)
  }, 1000)

  request.on('close', () => clearInterval(interval))
})

server.listen(3000, () => {
  const eventSource = new EventSource('http://localhost:3000')

  // Named event, delivered under its own type.
  eventSource.addEventListener('ping', (event) => {
    console.log('ping:', event.data)
  })

  // Unnamed event, delivered as 'message'.
  eventSource.onmessage = (event) => {
    console.log('message:', event.data)
  }
})
```

### Event: `'error'`

<!-- YAML
added: v6.5.0
changes:
  - version: v7.11.0
    pr-url: https://github.com/nodejs/undici/pull/4247
    description: The connection is no longer re-established after a network
                 error; the `EventSource` is closed instead.
-->

> Stability: 1 - Experimental

Emitted when the connection fails or is interrupted. The listener receives an
{Event}. When the failure is recoverable, the `EventSource` transitions back to
`CONNECTING` and retries after the reconnection time; when it is not, the
`EventSource` transitions to `CLOSED` and does not reconnect.

```mjs
import { EventSource } from 'undici'

const eventSource = new EventSource('http://localhost:3000')
eventSource.onerror = () => {
  if (eventSource.readyState === EventSource.CLOSED) {
    console.log('connection closed')
  } else {
    console.log('reconnecting')
  }
}
```

[WHATWG-conformant]: https://html.spec.whatwg.org/multipage/server-sent-events.html#server-sent-events
[`Dispatcher`]: Dispatcher.md#class-dispatcher
[`addEventListener()`]: https://developer.mozilla.org/en-US/docs/Web/API/EventTarget/addEventListener
[server-sent events]: https://developer.mozilla.org/en-US/docs/Web/API/Server-sent_events/Using_server-sent_events
