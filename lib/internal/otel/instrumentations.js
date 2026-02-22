'use strict';

const {
  ArrayPrototypePush,
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
  isActive,
  isModuleEnabled,
  getSpanStorage,
  getCollectorHost,
} = require('internal/otel/core');

function onHttpServerRequestStart({ request, response, socket, server }) {
  if (!isActive() || !isModuleEnabled('node:http')) return;

  const traceparent = request.headers?.traceparent;
  const parent = traceparent != null ? Span.extract(traceparent) : undefined;

  const method = request.method || 'GET';
  const span = new Span(method, SPAN_KIND_SERVER, {
    parent,
  });

  span.setAttribute('http.request.method', method);

  const rawUrl = request.url || '/';
  const qIdx = StringPrototypeIndexOf(rawUrl, '?');
  if (qIdx === -1) {
    span.setAttribute('url.path', rawUrl);
  } else {
    span.setAttribute('url.path', StringPrototypeSlice(rawUrl, 0, qIdx));
    span.setAttribute('url.query', StringPrototypeSlice(rawUrl, qIdx + 1));
  }

  span.setAttribute('url.scheme', socket?.encrypted ? 'https' : 'http');
  span.setAttribute('network.protocol.version',
                    request.httpVersion || '1.1');

  const host = request.headers?.host;
  if (host) {
    span.setAttribute('server.address', host);
  }

  request[kSpan] = span;

  const storage = getSpanStorage();
  storage.enterWith(span);

  request.on('close', () => {
    if (span.endTimeUnixNano === undefined) {
      span.setStatus(STATUS_ERROR, 'request closed before response');
      span.end();
    }
  });
}

function onHttpServerResponseFinish({ request, response }) {
  const span = request[kSpan];
  if (span == null) return;

  const statusCode = response.statusCode || 200;
  span.setAttribute('http.response.status_code', statusCode);

  if (statusCode >= 500) {
    span.setStatus(STATUS_ERROR, `HTTP ${statusCode}`);
    span.setAttribute('error.type', `${statusCode}`);
  }

  span.end();
}

function onHttpClientRequestCreated({ request }) {
  if (!isActive() || !isModuleEnabled('node:http')) return;

  if (request.getHeader('host') === getCollectorHost()) return;

  const storage = getSpanStorage();
  const parent = storage.getStore();

  const method = request.method || 'GET';
  const span = new Span(
    method,
    SPAN_KIND_CLIENT,
    { parent },
  );

  span.setAttribute('http.request.method', method);
  span.setAttribute('server.address', request.host || '');

  const port = request.socket?.remotePort || request.port;
  if (port != null) {
    span.setAttribute('server.port', port);
  }

  const url = `${request.protocol}//${request.host}${request.path}`;
  span.setAttribute('url.full', url);

  request[kSpan] = span;

  request.setHeader('traceparent', span.inject());
}

function onHttpClientResponseFinish({ request, response }) {
  const span = request[kSpan];
  if (span == null) return;

  const statusCode = response.statusCode;
  span.setAttribute('http.response.status_code', statusCode);
  span.setAttribute('network.protocol.version',
                    response.httpVersion || '1.1');

  if (statusCode >= 400) {
    span.setStatus(STATUS_ERROR, `HTTP ${statusCode}`);
    span.setAttribute('error.type', `${statusCode}`);
  }

  span.end();
}

function onHttpClientRequestError({ request, error }) {
  const span = request[kSpan];
  if (span == null) return;

  span.setAttribute('error.type', error?.name || 'Error');
  span.setStatus(STATUS_ERROR, error?.message || 'unknown error');
  span.addEvent('exception', {
    'exception.type': error?.name || 'Error',
    'exception.message': error?.message || '',
    'exception.stacktrace': error?.stack || '',
  });
  span.end();
}

function onUndiciRequestCreate({ request }) {
  if (!isActive()) return;
  if (!isModuleEnabled('node:undici') &&
      !isModuleEnabled('node:fetch')) return;

  const storage = getSpanStorage();
  const parent = storage.getStore();

  const method = request.method || 'GET';
  const span = new Span(
    method,
    SPAN_KIND_CLIENT,
    { parent },
  );

  span.setAttribute('http.request.method', method);
  span.setAttribute('server.address', request.origin || '');

  const url = `${request.origin}${request.path}`;
  span.setAttribute('url.full', url);

  request[kSpan] = span;

  if (request.addHeader) {
    request.addHeader('traceparent', span.inject());
  }
}

function onUndiciRequestHeaders({ request, response }) {
  const span = request[kSpan];
  if (span == null) return;

  const statusCode = response.statusCode;
  span.setAttribute('http.response.status_code', statusCode);

  if (statusCode >= 400) {
    span.setStatus(STATUS_ERROR, `HTTP ${statusCode}`);
    span.setAttribute('error.type', `${statusCode}`);
  }

  span.end();
}

function onUndiciRequestError({ request, error }) {
  const span = request[kSpan];
  if (span == null) return;

  span.setAttribute('error.type', error?.name || 'Error');
  span.setStatus(STATUS_ERROR, error?.message || 'unknown error');
  span.addEvent('exception', {
    'exception.type': error?.name || 'Error',
    'exception.message': error?.message || '',
    'exception.stacktrace': error?.stack || '',
  });
  span.end();
}

function enableInstrumentations() {
  const subscriptions = [];

  function sub(channel, handler) {
    dc.subscribe(channel, handler);
    ArrayPrototypePush(subscriptions, [channel, handler]);
  }

  if (isModuleEnabled('node:http')) {
    sub('http.server.request.start', onHttpServerRequestStart);
    sub('http.server.response.finish', onHttpServerResponseFinish);
    sub('http.client.request.created', onHttpClientRequestCreated);
    sub('http.client.response.finish', onHttpClientResponseFinish);
    sub('http.client.request.error', onHttpClientRequestError);
  }

  if (isModuleEnabled('node:undici') || isModuleEnabled('node:fetch')) {
    sub('undici:request:create', onUndiciRequestCreate);
    sub('undici:request:headers', onUndiciRequestHeaders);
    sub('undici:request:error', onUndiciRequestError);
  }

  return subscriptions;
}

function disableInstrumentations(subscriptions) {
  for (let i = 0; i < subscriptions.length; i++) {
    try {
      dc.unsubscribe(subscriptions[i][0], subscriptions[i][1]);
    } catch {
      // Best-effort cleanup; continue unsubscribing remaining channels.
    }
  }
}

module.exports = {
  enableInstrumentations,
  disableInstrumentations,
};
