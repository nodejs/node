# Debug

<!--type=misc-->

Undici emits human-readable debug logs through Node.js' built-in
[`util.debuglog()`][]. The logs are disabled by default and are turned on by
listing one or more namespaces in the `NODE_DEBUG` environment variable. Because
`util.debuglog()` reads `NODE_DEBUG` once, when the namespace is first used, the
variable must be set before the process starts.

Each log line is prefixed with the namespace (uppercased) and the process id,
for example `UNDICI 16241: connecting to nodejs.org using https:h1`. Multiple
namespaces can be enabled at once by separating them with commas, for example
`NODE_DEBUG=undici,websocket`.

The following sections describe the namespaces exposed by undici.

## `undici`

Enables debug logs for the core undici library, covering connection setup,
request dispatch, responses, and trailers. The `fetch` and `websocket` APIs are
built on top of this core, so enabling `undici` also surfaces activity that
originates from them.

```console
$ NODE_DEBUG=undici node script.js
UNDICI 16241: connecting to nodejs.org using https:h1
UNDICI 16241: connecting to nodejs.org using https:h1
UNDICI 16241: connected to nodejs.org using https:h1
UNDICI 16241: sending request to GET https://nodejs.org/
UNDICI 16241: received response to GET https://nodejs.org/ - HTTP 307
UNDICI 16241: connecting to nodejs.org using https:h1
UNDICI 16241: trailers received from GET https://nodejs.org/
UNDICI 16241: connected to nodejs.org using https:h1
UNDICI 16241: sending request to GET https://nodejs.org/en
UNDICI 16241: received response to GET https://nodejs.org/en - HTTP 200
UNDICI 16241: trailers received from GET https://nodejs.org/en
```

## `fetch`

Enables debug logs scoped to the `fetch` API. The output mirrors the `undici`
namespace but is emitted only for requests made through [`fetch()`][], which
makes it useful for isolating `fetch` traffic from other undici activity.

```console
$ NODE_DEBUG=fetch node script.js
FETCH 16241: connecting to nodejs.org using https:h1
FETCH 16241: connecting to nodejs.org using https:h1
FETCH 16241: connected to nodejs.org using https:h1
FETCH 16241: sending request to GET https://nodejs.org/
FETCH 16241: received response to GET https://nodejs.org/ - HTTP 307
FETCH 16241: connecting to nodejs.org using https:h1
FETCH 16241: trailers received from GET https://nodejs.org/
FETCH 16241: connected to nodejs.org using https:h1
FETCH 16241: sending request to GET https://nodejs.org/en
FETCH 16241: received response to GET https://nodejs.org/en - HTTP 200
FETCH 16241: trailers received from GET https://nodejs.org/en
```

## `websocket`

Enables debug logs scoped to the [`WebSocket`][] API, covering the opening
handshake and connection lifecycle. When the `undici` or `fetch` namespace is
also enabled, some lines may appear under both namespaces because the handshake
is performed through the core library.

```console
$ NODE_DEBUG=websocket node script.js
WEBSOCKET 18309: connecting to echo.websocket.org using https:h1
WEBSOCKET 18309: connected to echo.websocket.org using https:h1
WEBSOCKET 18309: sending request to GET https://echo.websocket.org/
WEBSOCKET 18309: connection opened <ip_address>
```

[`WebSocket`]: WebSocket.md#class-websocket
[`fetch()`]: Fetch.md
[`util.debuglog()`]: https://nodejs.org/api/util.html#utildebuglogsection-callback
