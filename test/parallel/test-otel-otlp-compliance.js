'use strict';
// Flags: --expose-internals

// This test verifies that the OTLP/HTTP JSON export payload is compliant
// with the OpenTelemetry specification (opentelemetry-proto).

require('../common');
const assert = require('node:assert');
const { after, before, describe, it } = require('node:test');

const otel = require('internal/otel/core');
const { flush } = require('internal/otel/flush');
const {
  startOTelCollector,
  startHTTPServer,
  getSpans,
  httpGet,
} = require('../common/otel');

// Verify that an attribute is a KeyValue holding exactly one AnyValue field.
function assertAnyValue(attr) {
  assert.ok(typeof attr.key === 'string', 'attribute key must be a string');
  assert.ok(attr.value !== undefined, 'attribute value must be present');

  const fields = Object.keys(attr.value);
  assert.strictEqual(fields.length, 1,
                     `attribute value must have exactly one field, got: ${fields}`);

  const field = fields[0];
  assert.ok(
    ['stringValue', 'boolValue', 'intValue',
     'doubleValue', 'arrayValue', 'kvlistValue',
     'bytesValue'].includes(field),
    `unexpected AnyValue field: ${field}`,
  );

  // intValue must be a decimal string (int64 JSON encoding).
  if (field === 'intValue') {
    assert.ok(typeof attr.value.intValue === 'string',
              'intValue must be a string (int64 JSON encoding)');
    assert.match(attr.value.intValue, /^-?\d+$/);
  }
}

describe('OTLP/JSON spec compliance', () => {
  let collector;

  before(async () => {
    collector = await startOTelCollector();
    otel.start({ endpoint: `http://127.0.0.1:${collector.port}` });
  });

  after(() => collector.close());

  it('produces a spec-compliant ExportTraceServiceRequest', async () => {
    const server = await startHTTPServer();

    await httpGet(server.port, '/test');
    flush();
    const payload = await collector.next();
    const lastRequest = collector.requests.at(-1);

    await server.close();

    // === Content-Type ===
    assert.strictEqual(lastRequest.headers['content-type'], 'application/json');

    // === Top-level envelope ===
    assert.ok(Array.isArray(payload.resourceSpans),
              'resourceSpans must be an array');
    assert.strictEqual(payload.resourceSpans.length, 1);

    const resourceSpan = payload.resourceSpans[0];

    // === Resource ===
    assert.ok(resourceSpan.resource, 'resource must be present');
    assert.ok(Array.isArray(resourceSpan.resource.attributes),
              'resource.attributes must be an array');

    // Verify resource attributes are KeyValue format.
    for (const attr of resourceSpan.resource.attributes) {
      assertAnyValue(attr);
    }

    // Verify required resource attributes.
    const resourceAttrs = {};
    for (const a of resourceSpan.resource.attributes) {
      resourceAttrs[a.key] = a.value;
    }
    assert.ok(resourceAttrs['service.name'],
              'resource must have service.name');
    assert.ok(resourceAttrs['service.name'].stringValue,
              'service.name must be a string value');

    // === ScopeSpans ===
    assert.ok(Array.isArray(resourceSpan.scopeSpans),
              'scopeSpans must be an array');
    assert.strictEqual(resourceSpan.scopeSpans.length, 1);

    const scopeSpan = resourceSpan.scopeSpans[0];

    // === InstrumentationScope ===
    assert.ok(scopeSpan.scope, 'scope must be present');
    assert.ok(typeof scopeSpan.scope.name === 'string',
              'scope.name must be a string');
    assert.ok(typeof scopeSpan.scope.version === 'string',
              'scope.version must be a string');

    // === Spans ===
    assert.ok(Array.isArray(scopeSpan.spans), 'spans must be an array');
    assert.ok(scopeSpan.spans.length >= 1, 'must have at least 1 span');

    for (const span of scopeSpan.spans) {
      // --- traceId: 32-char lowercase hex string ---
      assert.ok(typeof span.traceId === 'string', 'traceId must be a string');
      assert.strictEqual(span.traceId.length, 32);
      assert.match(span.traceId, /^[0-9a-f]{32}$/,
                   'traceId must be lowercase hex');
      // Must not be all zeros.
      assert.notStrictEqual(span.traceId, '0'.repeat(32));

      // --- spanId: 16-char lowercase hex string ---
      assert.ok(typeof span.spanId === 'string', 'spanId must be a string');
      assert.strictEqual(span.spanId.length, 16);
      assert.match(span.spanId, /^[0-9a-f]{16}$/,
                   'spanId must be lowercase hex');
      assert.notStrictEqual(span.spanId, '0'.repeat(16));

      // --- name: non-empty string ---
      assert.ok(typeof span.name === 'string', 'name must be a string');
      assert.ok(span.name.length > 0, 'name must be non-empty');

      // --- kind: integer 1-5 (OTLP SpanKind enum, no 0/UNSPECIFIED) ---
      assert.ok(typeof span.kind === 'number', 'kind must be a number');
      assert.ok(Number.isInteger(span.kind), 'kind must be an integer');
      assert.ok(span.kind >= 1 && span.kind <= 5,
                `kind must be 1-5, got: ${span.kind}`);

      // --- timestamps: decimal strings of nanoseconds ---
      assert.ok(typeof span.startTimeUnixNano === 'string',
                'startTimeUnixNano must be a string');
      assert.match(span.startTimeUnixNano, /^\d+$/,
                   'startTimeUnixNano must be a decimal string');
      assert.ok(typeof span.endTimeUnixNano === 'string',
                'endTimeUnixNano must be a string');
      assert.match(span.endTimeUnixNano, /^\d+$/,
                   'endTimeUnixNano must be a decimal string');

      // endTime >= startTime.
      assert.ok(BigInt(span.endTimeUnixNano) >= BigInt(span.startTimeUnixNano),
                'endTimeUnixNano must be >= startTimeUnixNano');

      // Timestamps should be plausible (after 2020, before 2100).
      const startSec = Number(BigInt(span.startTimeUnixNano) / 1_000_000_000n);
      assert.ok(startSec > 1577836800, 'timestamp too old'); // 2020-01-01
      assert.ok(startSec < 4102444800, 'timestamp too far in future'); // 2100-01-01

      // --- parentSpanId: omitted for root, 16-char hex for child ---
      if (span.parentSpanId !== undefined) {
        assert.ok(typeof span.parentSpanId === 'string');
        assert.strictEqual(span.parentSpanId.length, 16);
        assert.match(span.parentSpanId, /^[0-9a-f]{16}$/);
      }

      // --- attributes: array of KeyValue (or omitted if empty) ---
      if (span.attributes !== undefined) {
        assert.ok(Array.isArray(span.attributes));
        for (const attr of span.attributes) {
          assertAnyValue(attr);
        }
      }

      // --- status: omitted when unset, or {code, message} ---
      if (span.status !== undefined) {
        assert.ok(typeof span.status === 'object');
        assert.ok(typeof span.status.code === 'number');
        assert.ok(Number.isInteger(span.status.code));
        assert.ok(span.status.code >= 0 && span.status.code <= 2,
                  `status.code must be 0-2, got: ${span.status.code}`);
        // If message is present, it must be a string.
        if (span.status.message !== undefined) {
          assert.ok(typeof span.status.message === 'string');
        }
      }

      // --- events: omitted if empty, or array of Event ---
      if (span.events !== undefined) {
        assert.ok(Array.isArray(span.events));
        for (const event of span.events) {
          assert.ok(typeof event.name === 'string');
          assert.ok(event.name.length > 0, 'event name must be non-empty');
          assert.ok(typeof event.timeUnixNano === 'string');
          assert.match(event.timeUnixNano, /^\d+$/);
          if (event.attributes !== undefined) {
            assert.ok(Array.isArray(event.attributes));
          }
        }
      }

      // --- No snake_case field names ---
      for (const key of Object.keys(span)) {
        assert.ok(!key.includes('_') || key === 'startTimeUnixNano' ||
                  key === 'endTimeUnixNano' || key === 'timeUnixNano',
                  `unexpected snake_case-style field: ${key}`);
      }
    }
  });

  it('omits status when unset (200 OK response)', async () => {
    const server = await startHTTPServer();

    await httpGet(server.port, '/ok');
    flush();
    const payload = await collector.next();

    await server.close();

    const serverSpan = getSpans(payload).find((s) => {
      return s.kind === 2 && s.attributes?.some(
        (a) => a.key === 'url.path' && a.value.stringValue === '/ok');
    });
    assert.ok(serverSpan);
    // For a 200 OK server span, status should be omitted (STATUS_UNSET).
    assert.strictEqual(serverSpan.status, undefined);
  });

  it('includes status with code 2 for error responses', async () => {
    const server = await startHTTPServer((req, res) => {
      res.writeHead(500);
      res.end('error');
    });

    await httpGet(server.port, '/fail');
    flush();
    const payload = await collector.next();

    await server.close();

    const serverSpan = getSpans(payload).find((s) => {
      return s.kind === 2 && s.attributes?.some(
        (a) => a.key === 'url.path' && a.value.stringValue === '/fail');
    });
    assert.ok(serverSpan);
    assert.ok(serverSpan.status, 'status must be present for error');
    assert.strictEqual(serverSpan.status.code, 2); // STATUS_CODE_ERROR
    assert.ok(typeof serverSpan.status.message === 'string');
  });

  it('omits empty attributes array', async () => {
    const server = await startHTTPServer();

    await httpGet(server.port, '/test');
    flush();
    const payload = await collector.next();

    await server.close();

    for (const span of getSpans(payload)) {
      // If attributes is present, it must be non-empty.
      if (span.attributes !== undefined) {
        assert.ok(span.attributes.length > 0,
                  'attributes must not be an empty array');
      }
      // Events should not be present unless there are actual events.
      if (span.events !== undefined) {
        assert.ok(span.events.length > 0,
                  'events must not be an empty array');
      }
    }
  });

  it('posts to /v1/traces endpoint', async () => {
    const server = await startHTTPServer();

    await httpGet(server.port, '/x');
    flush();
    await collector.next();

    await server.close();

    assert.strictEqual(collector.requests.at(-1).url, '/v1/traces');
  });
});
