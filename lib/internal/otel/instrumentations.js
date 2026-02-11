'use strict';

const {
  StringPrototypeIndexOf,
  StringPrototypeSlice,
} = primordials;

const dc = require('diagnostics_channel');
const {
  Span,
  SPAN_KIND_SERVER,
  SPAN_KIND_CLIENT,
  STATUS_ERROR,
  kSpan,
} = require('internal/otel/span');
const {
  isModuleEnabled,
  getSpanStorage,
  getCollectorHost,
} = require('internal/otel/core');

// diagnostics_channel does not isolate subscriber exceptions: a throw
// here propagates into the instrumented module. Handlers must never throw.

// Cached at subscription time to avoid a per-event AsyncLocalStorage
// lookup.
let spanStorage;

// Shared response handling for client spans (http and undici alike). The
// caller decides when the span ends.
function applyClientResponse(span, statusCode) {
  span.setAttribute('http.response.status_code', statusCode);

  if (statusCode >= 400) {
    span.setStatus(STATUS_ERROR, `HTTP ${statusCode}`);
    span.setAttribute('error.type', `${statusCode}`);
  }
}

// Shared error handling for client spans (http and undici alike).
function failClientSpan(span, error) {
  span.setAttribute('error.type', error?.name || 'Error');
  span.setStatus(STATUS_ERROR, error?.message || 'unknown error');
  span.addEvent('exception', {
    'exception.type': error?.name || 'Error',
    'exception.message': error?.message || '',
    'exception.stacktrace': error?.stack || '',
  });
  span.end();
}

function onHttpServerRequestStart({ request, socket }) {
  // Skip the export request itself. Without this, tracing a collector
  // hosted in the same process would feed itself indefinitely: every
  // export would produce a server span that is exported again.
  if (request.url === '/v1/traces' &&
      request.headers?.host === getCollectorHost()) {
    return;
  }

  const traceparent = request.headers?.traceparent;
  const tracestate = request.headers?.tracestate;
  const parent = traceparent != null ?
    Span.fromTraceparent(traceparent) : undefined;

  const method = request.method || 'GET';
  const rawUrl = request.url || '/';
  const qIdx = StringPrototypeIndexOf(rawUrl, '?');
  const path = qIdx === -1 ? rawUrl : StringPrototypeSlice(rawUrl, 0, qIdx);
  // Per the OpenTelemetry semantic conventions, span names must be low
  // cardinality. Node.js core has no route concept, so only the method is
  // used; url.path remains available as an attribute.
  const span = new Span(method, SPAN_KIND_SERVER, {
    parent,
    // The tracestate is forwarded verbatim; it is never parsed.
    tracestate,
  });

  span.setAttribute('http.request.method', method);
  span.setAttribute('url.path', path);
  if (qIdx !== -1) {
    span.setAttribute('url.query', StringPrototypeSlice(rawUrl, qIdx + 1));
  }

  span.setAttribute('url.scheme', socket?.encrypted ? 'https' : 'http');
  span.setAttribute('network.protocol.version',
                    request.httpVersion || '1.1');

  const host = request.headers?.host;
  if (host) {
    span.setAttribute('server.address', host);
  }

  // The span is stashed on the request so that the response handlers do
  // not need a second AsyncLocalStorage lookup to find it.
  request[kSpan] = span;

  // The span deliberately stays in the AsyncLocalStorage after the request
  // ends: work started while handling it (timers, outgoing requests from
  // response callbacks) still links to the server span as its parent.
  spanStorage.enterWith(span);

  request.on('close', () => {
    if (span.endTime === undefined) {
      span.setStatus(STATUS_ERROR, 'request closed before response');
      span.end();
    }
  });
}

function onHttpServerResponseFinish({ request, response }) {
  const span = request[kSpan];
  if (span == null) return;

  const statusCode = response.statusCode;
  span.setAttribute('http.response.status_code', statusCode);

  if (statusCode >= 500) {
    span.setStatus(STATUS_ERROR, `HTTP ${statusCode}`);
    span.setAttribute('error.type', `${statusCode}`);
  }

  span.end();
}

function onHttpClientRequestCreated({ request }) {
  if (request.getHeader('host') === getCollectorHost()) return;

  const parent = spanStorage.getStore();

  const method = request.method || 'GET';
  const span = new Span(
    method,
    SPAN_KIND_CLIENT,
    { parent },
  );

  span.setAttribute('http.request.method', method);

  const { protocol, host } = request;
  const path = request.path;
  if (host) {
    span.setAttribute('server.address', host);
  }

  const port = request.socket?.remotePort || request.port;
  if (port != null) {
    span.setAttribute('server.port', port);
  }

  span.setAttribute('url.full', `${protocol}//${host}${path}`);

  request[kSpan] = span;

  request.setHeader('traceparent', span.toTraceparent());
  if (span.tracestate) {
    request.setHeader('tracestate', span.tracestate);
  }
}

function onHttpClientResponseFinish({ request, response }) {
  const span = request[kSpan];
  if (span == null) return;

  span.setAttribute('network.protocol.version',
                    response.httpVersion || '1.1');
  applyClientResponse(span, response.statusCode);

  // End the span when the response body completes. 'end' only fires when
  // the body has been fully read; 'close' covers aborted responses.
  const endSpan = () => span.end();
  response.once('end', endSpan);
  response.once('close', endSpan);
}

function onClientRequestError({ request, error }) {
  const span = request[kSpan];
  if (span == null) return;

  failClientSpan(span, error);
}

// Undici's addHeader appends rather than replaces; update an existing
// header in place so that a caller-supplied value is overridden, not
// duplicated.
function setUndiciHeader(request, name, value) {
  const headers = request.headers;
  for (let i = 0; i < headers.length; i += 2) {
    if (headers[i] === name) {
      headers[i + 1] = value;
      return;
    }
  }
  request.addHeader(name, value);
}

function onUndiciRequestCreate({ request }) {
  const parent = spanStorage.getStore();

  const method = request.method || 'GET';
  const span = new Span(
    method,
    SPAN_KIND_CLIENT,
    { parent },
  );

  span.setAttribute('http.request.method', method);

  const { origin } = request;
  const path = request.path;
  if (origin) {
    span.setAttribute('server.address', origin);
  }
  span.setAttribute('url.full', `${origin}${path}`);

  request[kSpan] = span;

  if (request.addHeader) {
    setUndiciHeader(request, 'traceparent', span.toTraceparent());
    if (span.tracestate) {
      setUndiciHeader(request, 'tracestate', span.tracestate);
    }
  }
}

function onUndiciRequestHeaders({ request, response }) {
  const span = request[kSpan];
  if (span == null) return;

  applyClientResponse(span, response.statusCode);
}

// undici:request:trailers fires when the response completes, including its
// body, so the span duration covers the full transfer.
function onUndiciRequestTrailers({ request }) {
  const span = request[kSpan];
  if (span == null) return;

  span.end();
}

function enableInstrumentations() {
  spanStorage = getSpanStorage();

  if (isModuleEnabled('node:http')) {
    dc.subscribe('http.server.request.start', onHttpServerRequestStart);
    dc.subscribe('http.server.response.finish', onHttpServerResponseFinish);
    dc.subscribe('http.client.request.created', onHttpClientRequestCreated);
    dc.subscribe('http.client.response.finish', onHttpClientResponseFinish);
    dc.subscribe('http.client.request.error', onClientRequestError);
  }

  if (isModuleEnabled('node:undici') || isModuleEnabled('node:fetch')) {
    dc.subscribe('undici:request:create', onUndiciRequestCreate);
    dc.subscribe('undici:request:headers', onUndiciRequestHeaders);
    dc.subscribe('undici:request:trailers', onUndiciRequestTrailers);
    dc.subscribe('undici:request:error', onClientRequestError);
  }
}

module.exports = {
  enableInstrumentations,
};
