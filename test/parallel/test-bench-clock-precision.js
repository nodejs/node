// Flags: --experimental-bench --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');

const originalHrtimeBigint = process.hrtime.bigint;
process.hrtime.bigint = () => 1n;
const { bench, run } = require('node:bench');
process.hrtime.bigint = originalHrtimeBigint;

const completion = bench('zero duration', { samples: 1 }, (b) => {
  b.start();
  b.end(1);
});

(async () => {
  await run().toArray();
  const result = await completion;
  assert.strictEqual(result.error.code, 'ERR_INVALID_STATE');
  assert.match(result.error.message, /insufficient clock precision/);
  // The failed run sets the exit code; reset it so the test itself passes.
  assert.strictEqual(process.exitCode, 1);
  process.exitCode = 0;
})().then(common.mustCall());
