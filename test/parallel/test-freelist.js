'use strict';

// Flags: --expose-internals

require('../common');
const assert = require('assert');
const freelist = require('internal/freelist');

assert.strictEqual(typeof freelist, 'object');
assert.strictEqual(typeof freelist.FreeList, 'function');

const flist1 = new freelist.FreeList('flist1', 3, String);

// Allocating when empty, should not change the list size
var result = flist1.alloc('test');
assert.strictEqual(typeof result, 'string');
assert.strictEqual(result, 'test');
assert.strictEqual(flist1.list.length, 0);

// Exhaust the free list
assert(flist1.free('test1'));
assert(flist1.free('test2'));
assert(flist1.free('test3'));

// Now it should not return 'true', as max length is exceeded
assert.strictEqual(flist1.free('test4'), false);
assert.strictEqual(flist1.free('test5'), false);

// At this point 'alloc' should just return the stored values
assert.strictEqual(flist1.alloc(), 'test3');
assert.strictEqual(flist1.alloc(), 'test2');
assert.strictEqual(flist1.alloc(), 'test1');
