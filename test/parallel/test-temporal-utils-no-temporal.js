// Flags: --expose-internals --no-harmony-temporal --no-warnings

'use strict';

const common = require('../common');
const assert = require('assert');

common.allowGlobals(globalThis.Temporal = {
  Instant: class Instant {},
});

const instant = new Temporal.Instant();

// Module contents should be independent of global state at require time
const temporalUtils = require('internal/util/temporal');

assert.strictEqual(temporalUtils.hasTemporal, false);
assert.strictEqual(temporalUtils.isTemporalInstant(instant), false);
