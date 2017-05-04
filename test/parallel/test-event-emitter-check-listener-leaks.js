'use strict';
require('../common');
const assert = require('assert');
const events = require('events');

let e = new events.EventEmitter();

// default
for (let i = 0; i < 10; i++) {
  e.on('default', () => {});
}
assert.ok(!e._events['default'].hasOwnProperty('warned'));
e.on('default', () => {});
assert.ok(e._events['default'].warned);

// symbol
const symbol = Symbol('symbol');
e.setMaxListeners(1);
e.on(symbol, () => {});
assert.ok(!e._events[symbol].hasOwnProperty('warned'));
e.on(symbol, () => {});
assert.ok(e._events[symbol].hasOwnProperty('warned'));

// specific
e.setMaxListeners(5);
for (let i = 0; i < 5; i++) {
  e.on('specific', () => {});
}
assert.ok(!e._events['specific'].hasOwnProperty('warned'));
e.on('specific', () => {});
assert.ok(e._events['specific'].warned);

// only one
e.setMaxListeners(1);
e.on('only one', () => {});
assert.ok(!e._events['only one'].hasOwnProperty('warned'));
e.on('only one', () => {});
assert.ok(e._events['only one'].hasOwnProperty('warned'));

// unlimited
e.setMaxListeners(0);
for (let i = 0; i < 1000; i++) {
  e.on('unlimited', () => {});
}
assert.ok(!e._events['unlimited'].hasOwnProperty('warned'));

// process-wide
events.EventEmitter.defaultMaxListeners = 42;
e = new events.EventEmitter();

for (let i = 0; i < 42; ++i) {
  e.on('fortytwo', () => {});
}
assert.ok(!e._events['fortytwo'].hasOwnProperty('warned'));
e.on('fortytwo', () => {});
assert.ok(e._events['fortytwo'].hasOwnProperty('warned'));
delete e._events['fortytwo'].warned;

events.EventEmitter.defaultMaxListeners = 44;
e.on('fortytwo', () => {});
assert.ok(!e._events['fortytwo'].hasOwnProperty('warned'));
e.on('fortytwo', () => {});
assert.ok(e._events['fortytwo'].hasOwnProperty('warned'));

// but _maxListeners still has precedence over defaultMaxListeners
events.EventEmitter.defaultMaxListeners = 42;
e = new events.EventEmitter();
e.setMaxListeners(1);
e.on('uno', () => {});
assert.ok(!e._events['uno'].hasOwnProperty('warned'));
e.on('uno', () => {});
assert.ok(e._events['uno'].hasOwnProperty('warned'));

// chainable
assert.strictEqual(e, e.setMaxListeners(1));
