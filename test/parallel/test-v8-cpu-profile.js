'use strict';

require('../common');
const assert = require('assert');
const v8 = require('v8');

const handle = v8.startCpuProfile();
const profile = handle.stop();
assert.ok(!!profile);
// Call stop() again
assert.ok(handle.stop() === undefined);
