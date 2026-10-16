'use strict';
// Flags: --expose-internals

require('../common');
const assert = require('node:assert');
const { after, before, describe, it } = require('node:test');

const otel = require('internal/otel/core');
const {
  Span,
  SPAN_KIND_INTERNAL,
  STATUS_UNSET,
} = require('internal/otel/span');
const { flush } = require('internal/otel/flush');
const {
  startOTelCollector,
  getSpans,
} = require('../common/otel');

describe('Span internals coverage', () => {
  let collector;

  before(async () => {
    collector = await startOTelCollector();
    otel.start({ endpoint: `http://127.0.0.1:${collector.port}` });
  });

  after(() => collector.close());

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
    assert.ok(span.endTime !== undefined);
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

  it('a second end() call is a no-op and does not double-export', async () => {
    const span = new Span('double-end', SPAN_KIND_INTERNAL);
    assert.strictEqual(span.endTime, undefined);

    span.end();
    const firstEndTime = span.endTime;
    assert.ok(firstEndTime !== undefined);

    span.end();
    assert.strictEqual(span.endTime, firstEndTime);

    flush();

    const spans = getSpans(await collector.next());

    const matches = spans.filter((s) => s.name === 'double-end');
    assert.strictEqual(matches.length, 1);
  });
});
