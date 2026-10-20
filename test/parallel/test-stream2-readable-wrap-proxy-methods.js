'use strict';
const common = require('../common');
const assert = require('assert');
const { Readable } = require('stream');

// Readable.prototype.wrap() proxies the methods of the wrapped old-style
// stream onto the new Readable. Regression test for an off-by-one that made
// the proxy loop start at index 1, silently skipping the first own-enumerable
// key returned by Object.keys() — so a method happening to sit at that first
// position was never proxied.

// A minimal old-style stream whose *first* own-enumerable key is a method.
// `Object.keys()` preserves insertion order for string keys, so `firstMethod`
// is `streamKeys[0]` — exactly the slot the bug skipped.
const source = {
  firstMethod() { return `first:${this === source}`; },
  secondMethod() { return `second:${this === source}`; },
  on() { return this; },
  pause() {},
  resume() {},
};

assert.strictEqual(Object.keys(source)[0], 'firstMethod');

const wrapped = new Readable().wrap(source);

// The method at the first key must be proxied (was `undefined` before the fix).
assert.strictEqual(typeof wrapped.firstMethod, 'function');
assert.strictEqual(typeof wrapped.secondMethod, 'function');

// Proxied methods stay bound to the original stream.
assert.strictEqual(wrapped.firstMethod(), 'first:true');
assert.strictEqual(wrapped.secondMethod(), 'second:true');

// Existing Readable methods must not be clobbered by the proxying.
assert.strictEqual(wrapped.pause, Readable.prototype.pause);
assert.strictEqual(wrapped.resume, Readable.prototype.resume);

wrapped.on('end', common.mustNotCall());
wrapped.destroy();
