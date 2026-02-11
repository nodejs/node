# OpenTelemetry

<!--introduced_in=REPLACEME-->

<!-- type=misc -->

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

Node.js includes an experimental built-in [OpenTelemetry][] tracing subsystem.
When activated, the subsystem automatically creates spans for HTTP server and
client operations and exports them using the [OTLP/HTTP JSON][] protocol.

The subsystem is experimental and must be enabled with the
`--experimental-otel` flag. It is activated only via environment
variables, listed below. There is currently no programmatic API for
activating or configuring the subsystem, and no support for custom
instrumentations; these may be added in the future.

## Limitations

The subsystem is independent of the OpenTelemetry JavaScript packages. It
is not interoperable with `@opentelemetry/api`, and spans created by one
are not visible to the other. Running both in the same process produces
duplicate trace and span IDs. The built-in instrumentation also
overwrites the `traceparent` (and, when present, `tracestate`) header on
outgoing HTTP requests, discarding whatever a userland propagator may
have set. Users should run either the built-in subsystem or a userland
OpenTelemetry SDK, not both.

The subsystem runs in the main thread only. Worker threads
(`node:worker_threads`) are not traced, even though they inherit the
environment variables.

Buffered spans are exported periodically, when the internal buffer fills,
and when the event loop is about to drain. Spans that are still buffered
when `process.exit()` is called explicitly are lost, because explicit
exits do not run the exit-time flush.

## Environment variables

### `NODE_OTEL`

When set to `1`, activates the tracing subsystem using the default
collector endpoint (`http://localhost:4318`). Values other than `1` are
ignored. If `NODE_OTEL_ENDPOINT` is also set, it takes precedence for the
endpoint.

```bash
node --experimental-otel app.js  # NODE_OTEL=1 set in the environment
```

### `NODE_OTEL_ENDPOINT`

When set to a non-empty value, activates the tracing subsystem and directs
spans to the specified OTLP collector endpoint. The endpoint should be the
base URL of an OTLP/HTTP collector (e.g. `http://localhost:4318`) without
a path: any path present in the endpoint is replaced with `/v1/traces`,
and an endpoint that already ends with `/v1/traces` is used as is. When
only `NODE_OTEL=1` is set, the default collector endpoint
(`http://localhost:4318`) is used.

```bash
NODE_OTEL_ENDPOINT=http://collector.example.com:4318 \
  node --experimental-otel app.js
```

### `NODE_OTEL_FILTER`

Accepts a comma-separated list of core modules to instrument. When not set, all
supported modules are instrumented. For example, setting
`NODE_OTEL_FILTER=node:http` would enable tracing only for the `node:http`
module.

Supported module filter values:

* `node:http` — HTTP server and client operations
* `node:undici` — Undici HTTP client operations
* `node:fetch` — Fetch API operations (alias for undici)

### `NODE_OTEL_MAX_BUFFER_SIZE`

Maximum number of spans buffered in memory before an immediate flush to the
collector is triggered. Must be a positive integer. **Default:** `100`.

### `NODE_OTEL_FLUSH_INTERVAL`

Interval in milliseconds between periodic flushes of buffered spans to the
collector. Must be a positive integer. **Default:** `10000`.

### `OTEL_SERVICE_NAME`

Standard OpenTelemetry environment variable used to set the service name in
exported resource attributes. Defaults to `unknown_service:node`, per the
OpenTelemetry [semantic conventions][] for low-cardinality service names.

## Instrumented operations

When the subsystem is active, spans are automatically created for the
following operations. Per the OpenTelemetry [semantic conventions][], span
names must be low cardinality; Node.js core has no route concept, so spans
are named `{method}` and the request details live in the attributes.

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
response body completes or an error occurs.

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
`fetch()` or undici's `request()`. Error status, `exception` event behavior,
and span end timing are the same as for HTTP client spans above.

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
  requests, and child spans created during request processing inherit the
  trace ID. `traceparent` values using a version other than `00` are
  rejected, and a fresh trace is started instead. The `tracestate` header,
  when present, is attached to the span and forwarded verbatim to outgoing
  requests. It is never parsed or modified.
* **Outgoing requests**: The `traceparent` header is injected into outgoing
  HTTP and undici/fetch requests, together with the stored `tracestate`
  when one is present, enabling distributed tracing across services.

[OTLP/HTTP JSON]: https://opentelemetry.io/docs/specs/otlp/#otlphttp
[W3C Trace Context]: https://www.w3.org/TR/trace-context/
[OpenTelemetry]: https://opentelemetry.io/
[semantic conventions]: https://opentelemetry.io/docs/specs/semconv/http/http-spans/
