'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasTemporal) {
  common.skip('Temporal is not enabled');
}

// TODO(legendecas): bundle zoneinfo data for small ICU.
if (globalThis.Intl && !common.hasFullICU) {
  common.skip('Time zone support unavailable when built with small ICU');
}

// Use globalThis.Temporal to workaround linter complaints.
assert.strictEqual(typeof globalThis.Temporal, 'object');
const pdt = globalThis.Temporal.Instant.from('1969-07-20T20:17:00Z');
assert.strictEqual(pdt.toString(), '1969-07-20T20:17:00Z');
