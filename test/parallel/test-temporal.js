// Flags: --harmony-temporal

'use strict';
const common = require('../common');
const assert = require('assert');

if (!process.config.variables.v8_enable_temporal_support) {
  common.skip('Temporal is not enabled');
}

// Use globalThis.Temporal to workaround linter complaints.
assert.strictEqual(typeof globalThis.Temporal, 'object');
const pdt = globalThis.Temporal.PlainDateTime.from('1969-07-20T20:17:00');
assert.strictEqual(pdt.toString(), '1969-07-20T20:17:00');
