// Flags: --no-experimental-web-worker
'use strict';

// The Web Worker API is enabled by default, but can be opted out of.
// Refs: https://github.com/nodejs/node/pull/65323

require('../common');
const assert = require('assert');

assert.strictEqual(typeof Worker, 'undefined');
