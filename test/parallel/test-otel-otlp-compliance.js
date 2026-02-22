'use strict';
// Flags: --experimental-otel

// This test verifies that the OTLP/HTTP JSON export payload is compliant
// with the OpenTelemetry specification (opentelemetry-proto).

require('../common');
const assert = require('node:assert');
const http = require('node:http');
const { describe, it } = require('node:test');

const otel = require('node:otel');

describe('OTLP/JSON spec compliance', () => {
  it('produces a spec-compliant ExportTraceServiceRequest', async () => {
    let resolvePayload;
    const payloadReceived = new Promise((r) => { resolvePayload = r; });
    let receivedContentType;

    const collector = http.createServer((req, res) => {
      receivedContentType = req.headers['content-type'];
      let body = '';
      req.on('data', (chunk) => { body += chunk; });
      req.on('end', () => {
        res.writeHead(200);
        res.end();
        resolvePayload(JSON.parse(body));
      });
    });

    await new Promise((r) => collector.listen(0, r));

    otel.start({ endpoint: `http://127.0.0.1:${collector.address().port}` });

    const server = http.createServer((req, res) => {
      res.writeHead(200);
      res.end('ok');
    });

    await new Promise((r) => server.listen(0, r));

    await new Promise((resolve, reject) => {
      http.get(`http://127.0.0.1:${server.address().port}/test`, (res) => {
        res.resume();
        res.on('end', resolve);
      }).on('error', reject);
    });

    otel.stop();

    const payload = await payloadReceived;

    collector.close();
    server.close();

    // === Content-Type ===
    assert.strictEqual(receivedContentType, 'application/json');

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
      assert.ok(typeof attr.key === 'string', 'attribute key must be string');
      assert.ok(attr.value !== undefined, 'attribute value must be present');
      // Each value must have exactly one of the AnyValue fields.
      const valueFields = Object.keys(attr.value);
      assert.strictEqual(valueFields.length, 1,
                         `attribute value must have exactly one field, got: ${valueFields}`);
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
          assert.ok(typeof attr.key === 'string');
          assert.ok(attr.value !== undefined);

          // Verify AnyValue has exactly one field.
          const fields = Object.keys(attr.value);
          assert.strictEqual(fields.length, 1);

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
    let resolvePayload;
    const payloadReceived = new Promise((r) => { resolvePayload = r; });

    const collector = http.createServer((req, res) => {
      let body = '';
      req.on('data', (chunk) => { body += chunk; });
      req.on('end', () => {
        res.writeHead(200);
        res.end();
        resolvePayload(JSON.parse(body));
      });
    });

    await new Promise((r) => collector.listen(0, r));

    otel.start({ endpoint: `http://127.0.0.1:${collector.address().port}` });

    const server = http.createServer((req, res) => {
      res.writeHead(200);
      res.end('ok');
    });

    await new Promise((r) => server.listen(0, r));

    await new Promise((resolve, reject) => {
      http.get(`http://127.0.0.1:${server.address().port}/ok`, (res) => {
        res.resume();
        res.on('end', resolve);
      }).on('error', reject);
    });

    otel.stop();

    const payload = await payloadReceived;

    collector.close();
    server.close();

    const spans = payload.resourceSpans[0].scopeSpans[0].spans;
    // For a 200 OK server span, status should be omitted (STATUS_UNSET).
    const serverSpan = spans.find((s) => s.kind === 2);
    assert.ok(serverSpan);
    assert.strictEqual(serverSpan.status, undefined);
  });

  it('includes status with code 2 for error responses', async () => {
    let resolvePayload;
    const payloadReceived = new Promise((r) => { resolvePayload = r; });

    const collector = http.createServer((req, res) => {
      let body = '';
      req.on('data', (chunk) => { body += chunk; });
      req.on('end', () => {
        res.writeHead(200);
        res.end();
        resolvePayload(JSON.parse(body));
      });
    });

    await new Promise((r) => collector.listen(0, r));

    otel.start({ endpoint: `http://127.0.0.1:${collector.address().port}` });

    const server = http.createServer((req, res) => {
      res.writeHead(500);
      res.end('error');
    });

    await new Promise((r) => server.listen(0, r));

    await new Promise((resolve, reject) => {
      http.get(`http://127.0.0.1:${server.address().port}/fail`, (res) => {
        res.resume();
        res.on('end', resolve);
      }).on('error', reject);
    });

    otel.stop();

    const payload = await payloadReceived;

    collector.close();
    server.close();

    const spans = payload.resourceSpans[0].scopeSpans[0].spans;
    const serverSpan = spans.find((s) => s.kind === 2);
    assert.ok(serverSpan);
    assert.ok(serverSpan.status, 'status must be present for error');
    assert.strictEqual(serverSpan.status.code, 2); // STATUS_CODE_ERROR
    assert.ok(typeof serverSpan.status.message === 'string');
  });

  it('omits empty attributes array', async () => {
    let resolvePayload;
    const payloadReceived = new Promise((r) => { resolvePayload = r; });

    const collector = http.createServer((req, res) => {
      let body = '';
      req.on('data', (chunk) => { body += chunk; });
      req.on('end', () => {
        res.writeHead(200);
        res.end();
        resolvePayload(JSON.parse(body));
      });
    });

    await new Promise((r) => collector.listen(0, r));

    otel.start({ endpoint: `http://127.0.0.1:${collector.address().port}` });

    const server = http.createServer((req, res) => {
      res.writeHead(200);
      res.end('ok');
    });

    await new Promise((r) => server.listen(0, r));

    await new Promise((resolve, reject) => {
      http.get(`http://127.0.0.1:${server.address().port}/test`, (res) => {
        res.resume();
        res.on('end', resolve);
      }).on('error', reject);
    });

    otel.stop();

    const payload = await payloadReceived;

    collector.close();
    server.close();

    const spans = payload.resourceSpans[0].scopeSpans[0].spans;
    for (const span of spans) {
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
    let receivedPath;
    let resolveRequest;
    const requestReceived = new Promise((r) => { resolveRequest = r; });

    const collector = http.createServer((req, res) => {
      receivedPath = req.url;
      req.on('data', () => {});
      req.on('end', () => {
        res.writeHead(200);
        res.end();
        resolveRequest();
      });
    });

    await new Promise((r) => collector.listen(0, r));

    otel.start({ endpoint: `http://127.0.0.1:${collector.address().port}` });

    const server = http.createServer((req, res) => {
      res.writeHead(200);
      res.end('ok');
    });

    await new Promise((r) => server.listen(0, r));

    await new Promise((resolve, reject) => {
      http.get(`http://127.0.0.1:${server.address().port}/x`, (res) => {
        res.resume();
        res.on('end', resolve);
      }).on('error', reject);
    });

    otel.stop();
    await requestReceived;

    collector.close();
    server.close();

    assert.strictEqual(receivedPath, '/v1/traces');
  });
});
