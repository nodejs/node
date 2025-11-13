// Flags: --harmony-temporal

'use strict';
const common = require('../common');
const assert = require('assert');

if (!process.config.variables.v8_enable_temporal_support) {
  common.skip('Temporal is not enabled');
}

// TODO(legendecas): bundle zoneinfo data for small ICU and without ICU builds.
if (!common.hasFullICU) {
  common.skip('Time zone support unavailable when not built with full ICU');
}

// Use globalThis.Temporal to workaround linter complaints.
assert.strictEqual(typeof globalThis.Temporal, 'object');
const pdt = globalThis.Temporal.Instant.from('1969-07-20T20:17:00Z');
assert.strictEqual(pdt.toString(), '1969-07-20T20:17:00Z');
