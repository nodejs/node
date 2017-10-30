'use strict';

const common = require('../common');
const EventEmitter = require('events');
const assert = require('assert');

const ee = new EventEmitter();
const foo = Symbol('foo');
const listener = common.mustCall();

ee.on(foo, listener);
assert.deepStrictEqual(ee.listeners(foo), [listener]);

ee.emit(foo);

ee.removeAllListeners();
assert.deepStrictEqual(ee.listeners(foo), []);

ee.on(foo, listener);
assert.deepStrictEqual(ee.listeners(foo), [listener]);

ee.removeListener(foo, listener);
assert.deepStrictEqual(ee.listeners(foo), []);
