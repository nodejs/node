'use strict';

const {
  ArrayPrototypePush,
  DateNow,
  NumberParseInt,
  NumberPrototypeToString,
  RegExpPrototypeExec,
  StringPrototypePadStart,
  StringPrototypeSplit,
  Symbol,
} = primordials;

const { addSpan } = require('internal/otel/flush');
const { generateTraceId, generateSpanId } = require('internal/otel/id');
const { now } = require('internal/perf/utils');

// Span kind and status code values, per the SpanKind and StatusCode
// enums of the OpenTelemetry protocol:
// https://github.com/open-telemetry/opentelemetry-proto/blob/main/opentelemetry/proto/trace/v1/trace.proto
const SPAN_KIND_INTERNAL = 1;
const SPAN_KIND_SERVER = 2;
const SPAN_KIND_CLIENT = 3;

// (Status codes: 0 UNSET, 1 OK, 2 ERROR.)
const STATUS_UNSET = 0;
const STATUS_ERROR = 2;

const kSpan = Symbol('kOtelSpan');

const kHex32 = /^[0-9a-f]{32}$/;
const kHex16 = /^[0-9a-f]{16}$/;
const kHex2 = /^[0-9a-f]{2}$/;
const kAllZero32 = '00000000000000000000000000000000';
const kAllZero16 = '0000000000000000';

class Span {
  traceId;
  spanId;
  parentSpanId;
  name;
  kind;
  // Raw W3C tracestate header value, forwarded verbatim to child
  // requests. Empty when no tracestate was received.
  tracestate;
  // Timestamps are epoch milliseconds. The start is read from the wall
  // clock (Date.now()); the end is derived as start + a monotonic duration
  // (performance.now()), so that spans always have human-readable absolute
  // timestamps while remaining safe from negative durations when the
  // system clock steps backward mid-span. All timestamps are only converted
  // to OTLP nanosecond strings at export time, so that unsampled or
  // filtered spans never pay for the conversion.
  startTime;
  endTime;
  #startMonotonic;

  #attributes;
  #events;
  #status;
  #traceFlags;

  constructor(name, kind, options) {
    const parent = options?.parent;

    this.name = name;
    this.kind = kind;
    this.spanId = generateSpanId();
    // The attribute keys below are the complete set written by the
    // instrumentations (there is no public attribute API yet).
    // Preallocating them gives every span's attribute object the same
    // static shape, so writes never trigger hidden class transitions or
    // dictionary mode. Unused keys stay undefined and are skipped at
    // export time.
    this.#attributes = {
      '__proto__': null,
      'error.type': undefined,
      'http.request.method': undefined,
      'http.response.status_code': undefined,
      'network.protocol.version': undefined,
      'server.address': undefined,
      'server.port': undefined,
      'url.full': undefined,
      'url.path': undefined,
      'url.query': undefined,
      'url.scheme': undefined,
    };
    this.#events = [];
    this.#status = { code: STATUS_UNSET, message: '' };
    this.startTime = DateNow();
    this.#startMonotonic = now();

    if (parent != null) {
      this.traceId = parent.traceId;
      this.parentSpanId = parent.spanId;
      this.#traceFlags = parent.traceFlags;
    } else {
      this.traceId = generateTraceId();
      this.parentSpanId = '';
      this.#traceFlags = 0x01; // Sampled by default.
    }
    // An explicit tracestate wins; children inherit their parent's.
    this.tracestate = options?.tracestate || parent?.tracestate || '';
  }

  get traceFlags() {
    return this.#traceFlags;
  }

  setAttribute(key, value) {
    this.#attributes[key] = value;
    return this;
  }

  getAttributes() {
    return this.#attributes;
  }

  addEvent(name, attributes) {
    // There is no public API for span events yet and the only internal
    // producer adds at most one exception event per span. Once events are
    // exposed through a public API, a per-span cap will be needed.
    // TODO(bengl): Cap the number of events per span when addEvent becomes
    // part of a public API.
    ArrayPrototypePush(this.#events, {
      name,
      // Anchored to the span's start like endTime, for the same clock-
      // step safety.
      time: this.startTime + (now() - this.#startMonotonic),
      attributes: attributes || { __proto__: null },
    });
    return this;
  }

  getEvents() {
    return this.#events;
  }

  get status() {
    return this.#status;
  }

  setStatus(code, message) {
    this.#status = { code, message: message || '' };
    return this;
  }

  end() {
    if (this.endTime !== undefined) return; // Already ended.
    this.endTime = this.startTime + (now() - this.#startMonotonic);

    // Only export sampled spans.
    if (this.#traceFlags & 0x01) {
      addSpan(this);
    }
  }

  // The counterpart of static fromTraceparent(): produces the outgoing
  // header value; the caller writes it into the request.
  // Format: {version}-{trace-id}-{span-id}-{trace-flags}
  toTraceparent() {
    // The common case is a sampled root span (flags 0x01).
    const flags = this.#traceFlags === 0x01 ?
      '01' :
      StringPrototypePadStart(
        NumberPrototypeToString(this.#traceFlags, 16), 2, '0');
    return `00-${this.traceId}-${this.spanId}-${flags}`;
  }

  // The counterpart of toTraceparent(): parses an incoming header value
  // into a remote parent. Returns null when invalid per the W3C spec.
  static fromTraceparent(traceparentHeader) {
    if (typeof traceparentHeader !== 'string') {
      return null;
    }

    const parts = StringPrototypeSplit(traceparentHeader, '-');
    if (parts.length !== 4) return null;

    // Only version 00 is defined; reject everything else.
    if (parts[0] !== '00') return null;
    const traceId = parts[1];
    const spanId = parts[2];
    const flags = parts[3];
    if (RegExpPrototypeExec(kHex32, traceId) === null) return null;
    if (RegExpPrototypeExec(kHex16, spanId) === null) return null;
    if (RegExpPrototypeExec(kHex2, flags) === null) return null;

    if (traceId === kAllZero32) return null;
    if (spanId === kAllZero16) return null;

    return {
      __proto__: null,
      traceId,
      spanId,
      traceFlags: NumberParseInt(flags, 16),
    };
  }
}

module.exports = {
  Span,
  SPAN_KIND_INTERNAL,
  SPAN_KIND_SERVER,
  SPAN_KIND_CLIENT,
  STATUS_UNSET,
  STATUS_ERROR,
  kSpan,
};
