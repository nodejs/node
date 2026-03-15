'use strict';
// Flags: --experimental-otel --expose-internals

require('../common');
const assert = require('node:assert');
const { describe, it } = require('node:test');

const { generateTraceId, generateSpanId } = require('internal/otel/id');

describe('otel ID generation buffer refill', () => {
  it('generates valid IDs after exhausting the random buffer', () => {
    // The random buffer is 4096 bytes. Each trace ID uses 16 bytes, so
    // 257 trace IDs (257 * 16 = 4112) will force at least one refill.
    const traceIds = new Set();
    for (let i = 0; i < 300; i++) {
      const id = generateTraceId();
      assert.strictEqual(id.length, 32);
      assert.match(id, /^[0-9a-f]{32}$/);
      traceIds.add(id);
    }
    assert.strictEqual(traceIds.size, 300);
  });

  it('generates valid span IDs after exhausting the random buffer', () => {
    // Each span ID uses 8 bytes, so 513 span IDs (513 * 8 = 4104) will
    // force at least one refill.
    const spanIds = new Set();
    for (let i = 0; i < 600; i++) {
      const id = generateSpanId();
      assert.strictEqual(id.length, 16);
      assert.match(id, /^[0-9a-f]{16}$/);
      spanIds.add(id);
    }
    assert.strictEqual(spanIds.size, 600);
  });
});
