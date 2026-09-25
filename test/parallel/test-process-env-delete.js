'use strict';
require('../common');
const assert = require('assert');

process.env.foo = 'foo';
assert.strictEqual(process.env.foo, 'foo');
process.env.foo = undefined;
assert.strictEqual(process.env.foo, 'undefined');

process.env.foo = 'foo';
assert.strictEqual(process.env.foo, 'foo');
delete process.env.foo;
assert.strictEqual(process.env.foo, undefined);
