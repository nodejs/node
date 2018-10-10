'use strict';

// Flags: --expose-internals

require('../common');
const assert = require('assert');
const { FreeList } = require('internal/freelist');

assert.strictEqual(typeof FreeList, 'function');

const flist1 = new FreeList('flist1', 3, Object);

// Allocating when empty, should not change the list size
const result = flist1.alloc();
assert.strictEqual(typeof result, 'object');
assert.strictEqual(flist1.list.length, 0);

// Exhaust the free list
assert(flist1.free({ id: 'test1' }));
assert(flist1.free({ id: 'test2' }));
assert(flist1.free({ id: 'test3' }));

// Now it should not return 'true', as max length is exceeded
assert.strictEqual(flist1.free({ id: 'test4' }), false);
assert.strictEqual(flist1.free({ id: 'test5' }), false);

// At this point 'alloc' should just return the stored values
assert.strictEqual(flist1.alloc().id, 'test3');
assert.strictEqual(flist1.alloc().id, 'test2');
assert.strictEqual(flist1.alloc().id, 'test1');
