'use strict';
// Flags: --expose_internals

require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const config = internalBinding('config');

console.log(config, process.argv);

assert.strictEqual(typeof require('internal/freelist').FreeList, 'function');
assert.strictEqual(config.exposeInternals, true);
