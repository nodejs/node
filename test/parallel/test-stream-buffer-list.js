// Flags: --expose_internals
'use strict';
require('../common');
const assert = require('assert');
const BufferList = require('internal/streams/BufferList');
const util = require('util');

// Test empty buffer list.
const emptyList = new BufferList();

emptyList.shift();
assert.deepStrictEqual(emptyList, new BufferList());

assert.strictEqual(emptyList.join(','), '');

assert.deepStrictEqual(emptyList.concat(0), Buffer.alloc(0));

// Test buffer list with one element.
const list = new BufferList();
list.push('foo');

assert.strictEqual(list.concat(1), 'foo');

assert.strictEqual(list.join(','), 'foo');

const shifted = list.shift();
assert.strictEqual(shifted, 'foo');
assert.deepStrictEqual(list, new BufferList());

const tmp = util.inspect.defaultOptions.colors;
util.inspect.defaultOptions = { colors: true };
assert.strictEqual(
  util.inspect(list),
  'BufferList { length: \u001b[33m0\u001b[39m }');
util.inspect.defaultOptions = { colors: tmp };
