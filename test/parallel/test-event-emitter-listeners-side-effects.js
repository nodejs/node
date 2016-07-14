'use strict';

const common = require('../common');
var assert = require('assert');

var EventEmitter = require('events').EventEmitter;

var e = new EventEmitter();
var fl;  // foo listeners

fl = e.listeners('foo');
assert(Array.isArray(fl));
assert(fl.length === 0);
assert(!(e._events instanceof Object));
assert.deepStrictEqual(Object.keys(e._events), []);

e.on('foo', common.fail);
fl = e.listeners('foo');
assert(e._events.foo === common.fail);
assert(Array.isArray(fl));
assert(fl.length === 1);
assert(fl[0] === common.fail);

e.listeners('bar');

e.on('foo', assert.ok);
fl = e.listeners('foo');

assert(Array.isArray(e._events.foo));
assert(e._events.foo.length === 2);
assert(e._events.foo[0] === common.fail);
assert(e._events.foo[1] === assert.ok);

assert(Array.isArray(fl));
assert(fl.length === 2);
assert(fl[0] === common.fail);
assert(fl[1] === assert.ok);

console.log('ok');
