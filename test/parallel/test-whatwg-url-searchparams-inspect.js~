'use strict';

require('../common');
const assert = require('assert');
const util = require('util');
const URLSearchParams = require('url').URLSearchParams;

// Tests below are not from WPT.
const sp = new URLSearchParams('?a=a&b=b&b=c');
assert.strictEqual(util.inspect(sp),
                   "URLSearchParams { 'a' => 'a', 'b' => 'b', 'b' => 'c' }");
assert.strictEqual(util.inspect(sp.keys()),
                   "URLSearchParamsIterator { 'a', 'b', 'b' }");
assert.strictEqual(util.inspect(sp.values()),
                   "URLSearchParamsIterator { 'a', 'b', 'c' }");
assert.strictEqual(util.inspect(sp.keys(), { breakLength: 1 }),
                   "URLSearchParamsIterator {\n  'a',\n  'b',\n  'b' }");

const iterator = sp.entries();
assert.strictEqual(util.inspect(iterator),
                   "URLSearchParamsIterator { [ 'a', 'a' ], [ 'b', 'b' ], " +
                                             "[ 'b', 'c' ] }");
iterator.next();
assert.strictEqual(util.inspect(iterator),
                   "URLSearchParamsIterator { [ 'b', 'b' ], [ 'b', 'c' ] }");
iterator.next();
iterator.next();
assert.strictEqual(util.inspect(iterator),
                   'URLSearchParamsIterator {  }');
