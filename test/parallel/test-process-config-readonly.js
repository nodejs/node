'use strict';

require('../common');
const assert = require('assert');

const config = process.config;

assert(config);
assert(config.variables);

// These throw because the objects are frozen.
assert.throws(() => process.config = {}, TypeError);
assert.throws(() => process.config.a = 1, TypeError);
assert.throws(() => process.config.variables.a = 1, TypeError);
