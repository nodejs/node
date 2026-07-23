// Flags: --expose-internals --no-warnings

'use strict';

const common = require('../common');
const assert = require('assert');

if (!common.hasTemporal) {
  common.skip('No Temporal support');
}

const instant = Temporal.Now.instant();

delete globalThis.Temporal;
assert.strictEqual(typeof Temporal, 'undefined');

// Module contents should be independent of global state at require time
const temporalUtils = require('internal/util/temporal');

assert.strictEqual(temporalUtils.hasTemporal, true);
assert.strictEqual(instant.constructor, temporalUtils.TemporalInstant);
assert.strictEqual(instant.epochNanoseconds,
                   temporalUtils.TemporalInstantPrototypeGetEpochNanoseconds(instant));
assert.strictEqual(temporalUtils.isTemporalInstant(instant), true);
assert.strictEqual(temporalUtils.isTemporalZonedDateTime(instant), false);
