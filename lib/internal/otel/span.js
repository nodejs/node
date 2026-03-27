'use strict';

const {
  ArrayPrototypePush,
  BigInt,
  MathRound,
  NumberParseInt,
  NumberPrototypeToString,
  ObjectAssign,
  RegExpPrototypeExec,
  StringPrototypePadStart,
  StringPrototypeSplit,
  Symbol,
} = primordials;

const { addSpan } = require('internal/otel/flush');
const { generateTraceId, generateSpanId } = require('internal/otel/id');
const { now, getTimeOriginTimestamp } = require('internal/perf/utils');

const SPAN_KIND_INTERNAL = 1;
const SPAN_KIND_SERVER = 2;
const SPAN_KIND_CLIENT = 3;

const STATUS_UNSET = 0;
const STATUS_ERROR = 2;

const kSpan = Symbol('kOtelSpan');

const kHex32 = /^[0-9a-f]{32}$/;
const kHex16 = /^[0-9a-f]{16}$/;
const kHex2 = /^[0-9a-f]{2}$/;
const kAllZero32 = '00000000000000000000000000000000';
const kAllZero16 = '0000000000000000';

// Compute the Unix epoch time origin once, in nanoseconds as a string.
// getTimeOriginTimestamp() returns milliseconds since Unix epoch.
// now() returns milliseconds since timeOrigin.
// We combine them for nanosecond-precision absolute timestamps.
let timeOriginNs;
function getTimeOriginNs() {
  if (timeOriginNs === undefined) {
    // getTimeOriginTimestamp() returns ms since Unix epoch.
    // Multiply by 1e6 to get nanoseconds.
    const originMs = getTimeOriginTimestamp();
    timeOriginNs = BigInt(MathRound(originMs * 1e6));
  }
  return timeOriginNs;
}

function hrTimeToNanos() {
  const relativeMs = now();
  const ns = getTimeOriginNs() + BigInt(MathRound(relativeMs * 1e6));
  return `${ns}`;
}

class Span {
  traceId;
  spanId;
  parentSpanId;
  name;
  kind;
  startTimeUnixNano;
  endTimeUnixNano;

  #attributes;
  #events;
  #status;
  #traceFlags;

  constructor(name, kind, options) {
    const parent = options?.parent;

    this.name = name;
    this.kind = kind;
    this.spanId = generateSpanId();
    this.#attributes = { __proto__: null };
    this.#events = [];
    this.#status = { code: STATUS_UNSET, message: '' };
    this.startTimeUnixNano = hrTimeToNanos();

    if (parent != null) {
      this.traceId = parent.traceId;
      this.parentSpanId = parent.spanId;
      this.#traceFlags = parent.traceFlags;
    } else {
      this.traceId = generateTraceId();
      this.parentSpanId = '';
      this.#traceFlags = 0x01; // Sampled by default.
    }
  }

  get traceFlags() {
    return this.#traceFlags;
  }

  setAttribute(key, value) {
    this.#attributes[key] = value;
    return this;
  }

  setAttributes(attrs) {
    if (attrs != null) {
      ObjectAssign(this.#attributes, attrs);
    }
    return this;
  }

  getAttributes() {
    return this.#attributes;
  }

  addEvent(name, attributes) {
    ArrayPrototypePush(this.#events, {
      name,
      timeUnixNano: hrTimeToNanos(),
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
    if (this.endTimeUnixNano !== undefined) return; // Already ended.
    this.endTimeUnixNano = hrTimeToNanos();

    // Only export sampled spans.
    if (this.#traceFlags & 0x01) {
      addSpan(this);
    }
  }

  // Generate W3C traceparent header value.
  // Format: {version}-{trace-id}-{span-id}-{trace-flags}
  inject() {
    const flags = StringPrototypePadStart(
      NumberPrototypeToString(this.#traceFlags, 16), 2, '0');
    return `00-${this.traceId}-${this.spanId}-${flags}`;
  }

  // Parse a W3C traceparent header into a "fake" remote parent span.
  // Returns null if the header is invalid per the W3C Trace Context spec.
  static extract(traceparentHeader) {
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
