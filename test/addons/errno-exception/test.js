'use strict';
const common = require('../../common');
const assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);
const err = binding.errno();
assert.strictEqual(err.syscall, 'syscall');
assert.strictEqual(err.errno, 10);
assert.strictEqual(err.path, 'päth');
assert.ok(/^Error:\s\w+, some error msg 'päth'$/.test(err.toString()));
