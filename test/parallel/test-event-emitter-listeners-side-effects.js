'use strict';

require('../common');
const assert = require('assert');

const EventEmitter = require('events').EventEmitter;

const e = new EventEmitter();
let fl;  // foo listeners

fl = e.listeners('foo');
assert(Array.isArray(fl));
assert.strictEqual(fl.length, 0);
assert(!(e._events instanceof Object));
assert.deepStrictEqual(Object.keys(e._events), []);

e.on('foo', assert.fail);
fl = e.listeners('foo');
assert.strictEqual(e._events.foo, assert.fail);
assert(Array.isArray(fl));
assert.strictEqual(fl.length, 1);
assert.strictEqual(fl[0], assert.fail);

e.listeners('bar');

e.on('foo', assert.ok);
fl = e.listeners('foo');

assert(Array.isArray(e._events.foo));
assert.strictEqual(e._events.foo.length, 2);
assert.strictEqual(e._events.foo[0], assert.fail);
assert.strictEqual(e._events.foo[1], assert.ok);

assert(Array.isArray(fl));
assert.strictEqual(fl.length, 2);
assert.strictEqual(fl[0], assert.fail);
assert.strictEqual(fl[1], assert.ok);

console.log('ok');
