'use strict';

const common = require('../../common');

// Verify that the path argument to node::ErrnoException() can contain UTF-8
// characters.

const assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);
const err = binding.errno();

assert.strictEqual(err.syscall, 'syscall');
assert.strictEqual(err.errno, 10);
assert.strictEqual(err.path, 'päth');
assert.match(err.toString(), /^Error:\s\w+, some error msg 'päth'$/);
