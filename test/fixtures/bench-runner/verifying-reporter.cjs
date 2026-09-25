'use strict';

const assert = require('assert');

module.exports = async function* verify(source) {
  for await (const { type, data } of source) {
    if (type === 'bench:sample') {
      assert.strictEqual(typeof data.duration_ns, 'bigint');
      assert.strictEqual(typeof data.rate, 'number');
    }
    if (type === 'bench:complete' && data.error !== undefined) {
      assert(data.error instanceof Error);
      assert.strictEqual(data.error.code, 'ERR_BENCHMARK_FIXTURE');
      assert.deepStrictEqual(data.error.cause, { value: 42n });
    }
    if (type === 'bench:complete' && data.summary !== undefined) {
      assert(Number.isFinite(data.summary.medianConfidenceInterval.lower));
    }
    if (type === 'bench:summary') yield 'verified\n';
  }
};
