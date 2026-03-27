'use strict';

const {
  ArrayPrototypePush,
  DateNow,
  JSONStringify,
  NumberIsInteger,
  ObjectKeys,
  String,
  StringPrototypeEndsWith,
} = primordials;

const { Buffer } = require('buffer');
const http = require('http');
const https = require('https');
const { clearInterval, setInterval } = require('timers');
const { URL } = require('internal/url');
const { getEndpoint } = require('internal/otel/core');

const kDefaultMaxBufferSize = 100;
const kDefaultFlushIntervalMs = 10_000;
const kWarningThrottleMs = 30_000;

let spanBuffer = [];
let flushTimer = null;
let maxBufferSize = kDefaultMaxBufferSize;

let exportFailureCount = 0;
let lastExportWarningTime = 0;

let cachedResource = null;
let cachedScope = null;

function getResource() {
  cachedResource ??= {
    attributes: [
      { key: 'service.name',
        value: { stringValue: process.env.OTEL_SERVICE_NAME ||
                              `node-${process.pid}` } },
      { key: 'telemetry.sdk.name',
        value: { stringValue: 'nodejs-core' } },
      { key: 'telemetry.sdk.language',
        value: { stringValue: 'nodejs' } },
      { key: 'telemetry.sdk.version',
        value: { stringValue: process.version } },
      { key: 'process.runtime.name',
        value: { stringValue: 'nodejs' } },
      { key: 'process.runtime.version',
        value: { stringValue: process.version } },
      { key: 'process.pid',
        value: { intValue: String(process.pid) } },
    ],
  };
  return cachedResource;
}

function getScope() {
  cachedScope ??= {
    name: 'nodejs-core',
    version: process.version,
  };
  return cachedScope;
}

function encodeAttributeValue(value) {
  if (typeof value === 'string') {
    return { stringValue: value };
  }
  if (typeof value === 'number') {
    if (NumberIsInteger(value)) {
      return { intValue: String(value) };
    }
    return { doubleValue: value };
  }
  if (typeof value === 'boolean') {
    return { boolValue: value };
  }
  return { stringValue: String(value) };
}

function spanToOtlp(span) {
  // TODO(bengl): A lot of objects are created in here for all the atributes.
  // As a future optimization, we could hand-write the JSON encoding.
  const rawAttrs = span.getAttributes();
  const attrKeys = ObjectKeys(rawAttrs);
  const attributes = [];
  for (let i = 0; i < attrKeys.length; i++) {
    ArrayPrototypePush(attributes, {
      key: attrKeys[i],
      value: encodeAttributeValue(rawAttrs[attrKeys[i]]),
    });
  }

  const rawEvents = span.getEvents();
  const events = [];
  for (let i = 0; i < rawEvents.length; i++) {
    const event = rawEvents[i];
    const eventAttrs = [];
    const eventAttrKeys = ObjectKeys(event.attributes);
    for (let j = 0; j < eventAttrKeys.length; j++) {
      ArrayPrototypePush(eventAttrs, {
        key: eventAttrKeys[j],
        value: encodeAttributeValue(event.attributes[eventAttrKeys[j]]),
      });
    }
    const otlpEvent = {
      name: event.name,
      timeUnixNano: event.timeUnixNano,
    };
    if (eventAttrs.length > 0) {
      otlpEvent.attributes = eventAttrs;
    }
    ArrayPrototypePush(events, otlpEvent);
  }

  const otlpSpan = {
    traceId: span.traceId,
    spanId: span.spanId,
    name: span.name,
    kind: span.kind,
    startTimeUnixNano: span.startTimeUnixNano,
    endTimeUnixNano: span.endTimeUnixNano,
  };

  if (attributes.length > 0) {
    otlpSpan.attributes = attributes;
  }

  const status = span.status;
  if (status.code !== 0 || status.message) {
    otlpSpan.status = status;
  }

  if (span.parentSpanId) {
    otlpSpan.parentSpanId = span.parentSpanId;
  }

  if (events.length > 0) {
    otlpSpan.events = events;
  }

  return otlpSpan;
}

function addSpan(span) {
  ArrayPrototypePush(spanBuffer, span);
  if (spanBuffer.length >= maxBufferSize) {
    flush();
  }
}

function flush() {
  if (spanBuffer.length === 0) return;
  if (getEndpoint() == null) return;

  const spans = spanBuffer;
  spanBuffer = [];

  const otlpSpans = [];
  for (let i = 0; i < spans.length; i++) {
    try {
      ArrayPrototypePush(otlpSpans, spanToOtlp(spans[i]));
    } catch (err) {
      exportFailureCount++;
      emitExportWarningThrottled(
        `Failed to serialize span "${spans[i].name}": ${err.message}`);
    }
  }

  if (otlpSpans.length === 0) return;

  let payload;
  try {
    payload = JSONStringify({
      resourceSpans: [{
        resource: getResource(),
        scopeSpans: [{
          scope: getScope(),
          spans: otlpSpans,
        }],
      }],
    });
  } catch (err) {
    exportFailureCount++;
    emitExportWarningThrottled(
      `Failed to serialize ${otlpSpans.length} spans: ${err.message}`);
    return;
  }

  sendToCollector(payload);
}

function emitExportWarningThrottled(message) {
  const now = DateNow();
  if (now - lastExportWarningTime >= kWarningThrottleMs) {
    lastExportWarningTime = now;
    const suffix = exportFailureCount > 1 ?
      ` (${exportFailureCount} total failures)` : '';
    process.emitWarning(
      `${message}${suffix}`,
      'OTelExportWarning',
    );
  }
}

function sendToCollector(body) {
  const endpoint = getEndpoint();
  if (endpoint == null) return;

  try {
    let urlStr = endpoint;
    if (!StringPrototypeEndsWith(urlStr, '/v1/traces')) {
      urlStr = StringPrototypeEndsWith(urlStr, '/') ?
        urlStr + 'v1/traces' :
        urlStr + '/v1/traces';
    }

    const parsed = new URL(urlStr);

    if (parsed.protocol !== 'https:' && parsed.protocol !== 'http:') {
      exportFailureCount++;
      emitExportWarningThrottled(
        `Unsupported protocol "${parsed.protocol}" in OTLP endpoint; ` +
        'only http: and https: are supported');
      return;
    }

    const transport = parsed.protocol === 'https:' ? https : http;

    const req = transport.request({
      hostname: parsed.hostname,
      port: parsed.port,
      path: parsed.pathname,
      method: 'POST',
      headers: {
        'content-type': 'application/json',
        'content-length': Buffer.byteLength(body),
      },
    }, (res) => {
      // TODO(bengl): Once retry logic is added, parse the response body for
      // ExportTraceServiceResponse.partial_success.rejected_spans.
      res.resume();
      res.on('end', () => {
        if (res.statusCode >= 400) {
          exportFailureCount++;
          emitExportWarningThrottled(
            `OTLP collector responded with HTTP ${res.statusCode}`);
        }
      });
      res.on('error', (err) => {
        exportFailureCount++;
        emitExportWarningThrottled(
          `OTLP export response stream error: ${err.message}`);
      });
    });

    req.on('error', (err) => {
      exportFailureCount++;
      emitExportWarningThrottled(
        `Failed to export spans to ${endpoint}: ${err.message}`);
    });

    req.end(body);
  } catch (err) {
    exportFailureCount++;
    emitExportWarningThrottled(
      `Failed to export spans to ${endpoint}: ${err.message}`);
  }
}

function startFlusher(options) {
  if (flushTimer != null) return;
  maxBufferSize = options?.maxBufferSize ?? kDefaultMaxBufferSize;
  const interval = options?.flushInterval ?? kDefaultFlushIntervalMs;
  flushTimer = setInterval(flush, interval);
  flushTimer.unref();

  process.on('beforeExit', flush);
}

function stopFlusher() {
  if (flushTimer != null) {
    clearInterval(flushTimer);
    flushTimer = null;
  }
  process.removeListener('beforeExit', flush);
}

function resetCaches() {
  cachedResource = null;
  cachedScope = null;
  exportFailureCount = 0;
  lastExportWarningTime = 0;
  maxBufferSize = kDefaultMaxBufferSize;
  spanBuffer = [];
}

module.exports = {
  addSpan,
  flush,
  startFlusher,
  stopFlusher,
  resetCaches,
};
