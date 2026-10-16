'use strict';
// Flags: --expose-internals

require('../common');
const assert = require('node:assert');
const { describe, it } = require('node:test');

const { generateTraceId, generateSpanId } = require('internal/otel/id');

describe('otel ID generation buffer refill', () => {
  // The random buffer is 4096 bytes. Each trace ID uses 16 bytes and each
  // span ID 8 bytes, so generating enough IDs forces at least one refill.
  const cases = [
    ['trace', generateTraceId, 32, 300],
    ['span', generateSpanId, 16, 600],
  ];

  for (const [name, generate, idLength, count] of cases) {
    it(`generates valid ${name} IDs after exhausting the random buffer`, () => {
      const ids = new Set();
      for (let i = 0; i < count; i++) {
        const id = generate();
        assert.strictEqual(id.length, idLength);
        assert.match(id, new RegExp(`^[0-9a-f]{${idLength}}$`));
        ids.add(id);
      }
      assert.strictEqual(ids.size, count);
    });
  }
});
