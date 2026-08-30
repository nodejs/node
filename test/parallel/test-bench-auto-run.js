// Flags: --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const { bench } = require('node:bench');

const child = spawnSync(process.execPath, [
  '--no-warnings',
  '-e',
  'require("node:bench").bench("failure", () => { throw new Error(); })',
]);
assert.strictEqual(child.status, 1);

const completion = bench('automatic execution', common.mustCall((b) => {
  b.record({ duration_ns: 1n, operations: 1 });
}, 30));

completion.then(common.mustCall((result) => {
  assert.strictEqual(result.name, 'automatic execution');
  assert.strictEqual(result.samples.length, 30);
  assert.strictEqual(result.error, undefined);
  assert.strictEqual(result.skip, undefined);
  assert.strictEqual(result.summary.mean > 0, true);
}));
