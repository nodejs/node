'use strict';

const {
  ArrayPrototypePush,
  BigInt,
  DateNow,
  JSONStringify,
  MathRound,
  NumberIsInteger,
  ObjectKeys,
  String,
} = primordials;

const { Buffer } = require('buffer');
const http = require('http');
const https = require('https');
const { clearTimeout, setImmediate, setTimeout } = require('timers');
const { URL } = require('internal/url');
const { getEndpoint } = require('internal/otel/core');

const kDefaultMaxBufferSize = 100;
const kDefaultFlushIntervalMs = 10_000;
const kWarningThrottleMs = 30_000;
const kExportTimeoutMs = 10_000;

// One keep-alive socket per protocol: connections are reused, and
// batches queue behind it, bounding in-flight exports to a slow collector.
const kExportAgents = {
  '__proto__': null,
  'http:': new http.Agent({ keepAlive: true, maxSockets: 1 }),
  'https:': new https.Agent({ keepAlive: true, maxSockets: 1 }),
};

let spanBuffer = [];
let flushTimer = null; // One-shot timer; only set while spans are buffered.
let flushScheduled = false; // An off-thread flush is already scheduled.
let flushIntervalMs = kDefaultFlushIntervalMs;
let maxBufferSize = kDefaultMaxBufferSize;
let exportUrl = null; // Resolved once; the endpoint is fixed after start.

// Read once at activation time so that later env mutations cannot change
// the service name of exports. The default follows the OpenTelemetry
// semantic conventions: a low-cardinality fallback value, deliberately
// without the pid.
const kServiceName = process.env.OTEL_SERVICE_NAME ||
                    'unknown_service:node';

let exportFailureCount = 0;
let lastExportWarningTime = 0;

let cachedResource = null;
let cachedScope = null;

function getResource() {
  cachedResource ??= {
    attributes: [
      { key: 'service.name',
        value: { stringValue: kServiceName } },
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

// Converts a wall-clock epoch millisecond timestamp into an OTLP
// nanosecond string. Rounding to whole milliseconds before multiplying
// keeps the product exactly representable in double precision.
function timeToUnixNano(t) {
  return `${BigInt(MathRound(t)) * 1_000_000n}`;
}

function spanToOtlp(span) {
  // TODO(bengl): A lot of objects are created in here for all the atributes.
  // As a future optimization, we could hand-write the JSON encoding.
  const rawAttrs = span.getAttributes();
  const attrKeys = ObjectKeys(rawAttrs);
  const attributes = [];
  for (let i = 0; i < attrKeys.length; i++) {
    // Attribute values may be deferred functions that only run at export
    // time; values that resolve to undefined are omitted.
    // Unused preallocated keys stay undefined and are skipped.
    if (rawAttrs[attrKeys[i]] === undefined) continue;
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
      if (event.attributes[eventAttrKeys[j]] === undefined) continue;
      ArrayPrototypePush(eventAttrs, {
        key: eventAttrKeys[j],
        value: encodeAttributeValue(event.attributes[eventAttrKeys[j]]),
      });
    }
    const otlpEvent = {
      name: event.name,
      timeUnixNano: timeToUnixNano(event.time),
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
    startTimeUnixNano: timeToUnixNano(span.startTime),
    endTimeUnixNano: timeToUnixNano(span.endTime),
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
    // Defer the flush so serialization happens off the request path.
    if (!flushScheduled) {
      flushScheduled = true;
      setImmediate(() => {
        flushScheduled = false;
        flush();
      });
    }
  } else if (flushTimer == null) {
    // One-shot timer; only exists while spans are buffered.
    flushTimer = setTimeout(flush, flushIntervalMs);
    flushTimer.unref();
  }
}

function flush() {
  if (flushTimer != null) {
    clearTimeout(flushTimer);
    flushTimer = null;
  }
  if (spanBuffer.length === 0) return;
  if (getEndpoint() == null) return;

  const spans = spanBuffer;
  spanBuffer = [];

  const otlpSpans = [];
  for (let i = 0; i < spans.length; i++) {
    try {
      ArrayPrototypePush(otlpSpans, spanToOtlp(spans[i]));
    } catch (err) {
      warnExportFailure(
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
    warnExportFailure(
      `Failed to serialize ${otlpSpans.length} spans: ${err.message}`);
    return;
  }

  sendToCollector(payload);
}

// Emits a throttled OTelExportWarning and counts the failure. Warnings are
// throttled to at most one per kWarningThrottleMs.
function warnExportFailure(message) {
  exportFailureCount++;
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
    // The endpoint is fixed after start, so resolve the export URL once.
    // /v1/traces is the standard OTLP/HTTP traces path; an endpoint
    // already ending with it yields the same URL.
    exportUrl ??= new URL('/v1/traces', endpoint);
    const parsed = exportUrl;

    if (parsed.protocol !== 'https:' && parsed.protocol !== 'http:') {
      warnExportFailure(
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
      agent: kExportAgents[parsed.protocol],
      timeout: kExportTimeoutMs,
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
          warnExportFailure(
            `OTLP collector responded with HTTP ${res.statusCode}`);
        }
      });
      res.on('error', (err) => {
        warnExportFailure(
          `OTLP export response stream error: ${err.message}`);
      });
    });

    req.on('timeout', () => {
      // Destroying the request routes the failure into the (throttled)
      // warning path and frees the socket for the next batch.
      req.destroy();
    });

    req.on('error', (err) => {
      warnExportFailure(
        `Failed to export spans to ${endpoint}: ${err.message}`);
    });

    req.end(body);
  } catch (err) {
    warnExportFailure(
      `Failed to export spans to ${endpoint}: ${err.message}`);
  }
}

function startFlusher(options) {
  maxBufferSize = options?.maxBufferSize ?? kDefaultMaxBufferSize;
  flushIntervalMs = options?.flushInterval ?? kDefaultFlushIntervalMs;

  process.on('beforeExit', flush);
}

// Resets per-run state. Used by the test suite to isolate scenarios;
// tracing itself never resets once started.
function resetCaches() {
  if (flushTimer != null) {
    clearTimeout(flushTimer);
    flushTimer = null;
  }
  flushScheduled = false;
  flushIntervalMs = kDefaultFlushIntervalMs;
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
  resetCaches,
};
