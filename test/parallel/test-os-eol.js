'use strict';

const common = require('../common');
const assert = require('assert');
const os = require('os');

const eol = common.isWindows ? '\r\n' : '\n';

assert.strictEqual(os.EOL, eol);

// Test that the `Error` is a `TypeError` but do not check the message as it
// varies between different JavaScript engines.
assert.throws(function() { os.EOL = 123; }, TypeError);

const foo = 'foo';
Object.defineProperties(os, {
  EOL: {
    configurable: true,
    enumerable: true,
    writable: false,
    value: foo
  }
});
assert.strictEqual(os.EOL, foo);
