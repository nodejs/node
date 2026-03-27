'use strict';
// Flags: --experimental-otel --expose-internals

require('../common');
const assert = require('node:assert');
const http = require('node:http');
const { describe, it } = require('node:test');

const otel = require('node:otel');
const {
  Span,
  SPAN_KIND_INTERNAL,
  STATUS_UNSET,
} = require('internal/otel/span');

describe('Span internals coverage', () => {
  it('unsampled span does not call addSpan', () => {
    // Create a parent with traceFlags=0x00 (not sampled).
    const parent = {
      __proto__: null,
      traceId: 'a'.repeat(32),
      spanId: 'b'.repeat(16),
      traceFlags: 0x00,
    };

    const span = new Span('unsampled', SPAN_KIND_INTERNAL, { parent });
    assert.strictEqual(span.traceFlags, 0x00);

    // end() should not throw even though addSpan won't buffer it.
    span.end();
    assert.ok(span.endTimeUnixNano !== undefined);
  });

  it('setAttributes with null is a no-op', () => {
    const span = new Span('test', SPAN_KIND_INTERNAL);
    span.setAttribute('key', 'value');
    span.setAttributes(null);
    span.setAttributes(undefined);

    const attrs = span.getAttributes();
    assert.strictEqual(attrs.key, 'value');
    assert.strictEqual(Object.keys(attrs).length, 1);
  });

  it('addEvent without attributes uses empty object', () => {
    const span = new Span('test', SPAN_KIND_INTERNAL);
    span.addEvent('my-event');

    const events = span.getEvents();
    assert.strictEqual(events.length, 1);
    assert.strictEqual(events[0].name, 'my-event');
    assert.ok(events[0].attributes);
  });

  it('status defaults to UNSET', () => {
    const span = new Span('test', SPAN_KIND_INTERNAL);
    assert.strictEqual(span.status.code, STATUS_UNSET);
    assert.strictEqual(span.status.message, '');
  });

  it('second end() call is a no-op', () => {
    otel.start({ endpoint: 'http://127.0.0.1:1' });

    const span = new Span('test', SPAN_KIND_INTERNAL);
    assert.strictEqual(span.endTimeUnixNano, undefined);

    span.end();
    const firstEndTime = span.endTimeUnixNano;
    assert.ok(firstEndTime !== undefined);

    span.end();
    assert.strictEqual(span.endTimeUnixNano, firstEndTime);

    otel.stop();
  });

  it('does not export duplicate spans when end() called twice', async () => {
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

    const span = new Span('double-end', SPAN_KIND_INTERNAL);
    span.end();
    span.end();

    otel.stop();

    const payload = await payloadReceived;

    collector.close();

    const spans = payload.resourceSpans[0].scopeSpans[0].spans;

    const matches = spans.filter((s) => s.name === 'double-end');
    assert.strictEqual(matches.length, 1);
  });
});
