'use strict';
// Flags: --expose_internals

require('../common');
const assert = require('assert');
const config = process.binding('config');

console.log(config, process.argv);

assert.strictEqual(typeof require('internal/freelist'), 'function');
assert.strictEqual(config.exposeInternals, true);
