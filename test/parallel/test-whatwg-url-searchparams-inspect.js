'use strict';

require('../common');
const assert = require('assert');
const util = require('util');
const URL = require('url').URL;

// Until we export URLSearchParams
const m = new URL('http://example.org');
const URLSearchParams = m.searchParams.constructor;

const sp = new URLSearchParams('?a=a&b=b&b=c');
assert.strictEqual(util.inspect(sp),
                   "URLSearchParams { 'a' => 'a', 'b' => 'b', 'b' => 'c' }");
assert.strictEqual(util.inspect(sp.keys()),
                   "URLSearchParamsIterator { 'a', 'b', 'b' }");
assert.strictEqual(util.inspect(sp.values()),
                   "URLSearchParamsIterator { 'a', 'b', 'c' }");

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
