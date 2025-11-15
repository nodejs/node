// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('node:assert');
const { CustomEvent: internalCustomEvent } = require('internal/event_target');

// Global
assert.ok(CustomEvent);

assert.strictEqual(CustomEvent, internalCustomEvent);
