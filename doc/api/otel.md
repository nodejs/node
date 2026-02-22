# OpenTelemetry

<!--introduced_in=REPLACEME-->

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

<!-- source_link=lib/otel.js -->

The `node:otel` module provides built-in [OpenTelemetry][] tracing support for
Node.js core components. When enabled, it automatically creates spans for HTTP
server and client operations and exports them using the [OTLP/HTTP JSON][]
protocol.

To access it:

```cjs
const otel = require('node:otel');
```

```mjs
import otel from 'node:otel';
```

This module is only available under the `node:` scheme. It requires the
`--experimental-otel` CLI flag or one of the `NODE_OTEL` / `NODE_OTEL_ENDPOINT`
environment variables.

## Activation

There are two ways to activate OpenTelemetry tracing:

### Environment variables

Setting `NODE_OTEL=1` enables tracing with the default collector endpoint
(`http://localhost:4318`):

```bash
NODE_OTEL=1 node app.js
```

To use a custom collector endpoint, set `NODE_OTEL_ENDPOINT` instead (or in
addition):

```bash
NODE_OTEL_ENDPOINT=http://collector.example.com:4318 node app.js
```

Both variables also make the `node:otel` module available without requiring the
`--experimental-otel` flag.

### Programmatic API

```cjs
const otel = require('node:otel');

// Start with the default endpoint (http://localhost:4318):
otel.start();

// Or with a custom endpoint:
otel.start({ endpoint: 'http://collector.example.com:4318' });

// ... application code ...
otel.stop();
```

## Environment variables

### `NODE_OTEL`

When set to a non-empty value, enables the OpenTelemetry tracing subsystem
using the default collector endpoint (`http://localhost:4318`). If
`NODE_OTEL_ENDPOINT` is also set, it takes precedence.

### `NODE_OTEL_ENDPOINT`

When set to a non-empty value, enables the OpenTelemetry tracing subsystem and
directs spans to the specified OTLP collector endpoint. The endpoint should be
the base URL of an
OTLP/HTTP collector (e.g. `http://localhost:4318`). The `/v1/traces` path is
appended automatically.

### `NODE_OTEL_FILTER`

Accepts a comma-separated list of core modules to instrument. When not set, all
supported modules are instrumented. For example, setting
`NODE_OTEL_FILTER=node:http` would enable tracing only for the `node:http`
module.

Supported module filter values:

* `node:http` — HTTP server and client operations
* `node:undici` — Undici HTTP client operations
* `node:fetch` — Fetch API operations (alias for undici)

### `OTEL_SERVICE_NAME`

Standard OpenTelemetry environment variable used to set the service name in
exported resource attributes. Defaults to `node-<pid>`.

## `otel.start(options)`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `endpoint` {string} The OTLP collector endpoint URL.
    **Default:** `'http://localhost:4318'`.
  * `filter` {string|string\[]} Optional comma-separated string or array of
    core module names to instrument (e.g. `['node:http']` or
    `'node:http,node:undici'`). When omitted, all supported modules are
    instrumented.
  * `maxBufferSize` {number} Maximum number of spans buffered in memory before
    triggering an immediate flush to the collector. Must be a positive integer.
    **Default:** `100`.
  * `flushInterval` {number} Interval in milliseconds between periodic flushes
    of buffered spans to the collector. Must be a positive integer.
    **Default:** `10000`.

Enables the OpenTelemetry tracing subsystem. Spans are created for supported
core module operations and exported to the specified collector using the
OTLP/HTTP JSON protocol.

If tracing is already active, calling `start()` will stop the current session
and start a new one with the provided options.

## `otel.stop()`

<!-- YAML
added: REPLACEME
-->

Disables the OpenTelemetry tracing subsystem. Any buffered spans are sent to
the collector before stopping. Because the export is asynchronous, delivery is
not confirmed before `stop()` returns. After calling `stop()`, no new spans are
created.

Calling `stop()` when tracing is not active is a no-op.

## `otel.active`

<!-- YAML
added: REPLACEME
-->

* {boolean}

Returns `true` if the OpenTelemetry tracing subsystem is currently active.

## Instrumented operations

When tracing is active, spans are automatically created for the following
operations:

### HTTP server

A span with kind `SERVER` is created for each incoming HTTP request. The span
starts when the request is received and ends when the response finishes. If the
client disconnects before the response completes, the span ends with an error
status.

Server spans receive error status (`STATUS_ERROR`) for 5xx response codes. 4xx
responses are not treated as server errors per OpenTelemetry semantic
conventions.

Attributes set on server spans:

| Attribute                   | Description                      | Condition                     |
| --------------------------- | -------------------------------- | ----------------------------- |
| `http.request.method`       | HTTP method (e.g. `GET`, `POST`) | Always                        |
| `url.path`                  | Request URL path (without query) | Always                        |
| `url.query`                 | Query string (without `?`)       | When query string is present  |
| `url.scheme`                | `http` or `https`                | Always                        |
| `server.address`            | Host header value                | When `Host` header is present |
| `network.protocol.version`  | HTTP version (e.g. `1.1`)        | Always                        |
| `http.response.status_code` | Response status code             | When response finishes        |
| `error.type`                | HTTP status code as string       | On 5xx responses              |

### HTTP client

A span with kind `CLIENT` is created for each outgoing HTTP request made via
`node:http`. The span starts when the request is created and ends when the
response finishes or an error occurs.

Client spans receive error status (`STATUS_ERROR`) for 4xx and 5xx response
codes. On connection errors, an `exception` event is added to the span with
`exception.type`, `exception.message`, and `exception.stacktrace` attributes.

Attributes set on client spans:

| Attribute                   | Description               | Condition                 |
| --------------------------- | ------------------------- | ------------------------- |
| `http.request.method`       | HTTP method               | Always                    |
| `url.full`                  | Full request URL          | Always                    |
| `server.address`            | Target host               | Always                    |
| `server.port`               | Target port               | When available            |
| `http.response.status_code` | Response status code      | When response is received |
| `network.protocol.version`  | HTTP version              | When response is received |
| `error.type`                | Status code or error name | On 4xx/5xx or errors      |

### Undici/Fetch client

A span with kind `CLIENT` is created for each outgoing request made via
`fetch()` or undici's `request()`.

Error status and `exception` event behavior is the same as for HTTP client
spans above.

Attributes set on undici/fetch client spans:

| Attribute                   | Description               | Condition                 |
| --------------------------- | ------------------------- | ------------------------- |
| `http.request.method`       | HTTP method               | Always                    |
| `url.full`                  | Full request URL          | Always                    |
| `server.address`            | Target origin             | Always                    |
| `http.response.status_code` | Response status code      | When response is received |
| `error.type`                | Status code or error name | On 4xx/5xx or errors      |

## W3C Trace Context propagation

The tracing subsystem automatically propagates [W3C Trace Context][] across HTTP
boundaries:

* **Incoming requests**: The `traceparent` header is read from incoming HTTP
  requests. Child spans created during request processing inherit the trace ID.
* **Outgoing requests**: The `traceparent` header is injected into outgoing HTTP
  and undici/fetch requests, enabling distributed tracing across services.

[OTLP/HTTP JSON]: https://opentelemetry.io/docs/specs/otlp/#otlphttp
[OpenTelemetry]: https://opentelemetry.io/
[W3C Trace Context]: https://www.w3.org/TR/trace-context/
