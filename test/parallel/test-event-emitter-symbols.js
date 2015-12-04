'use strict';

const common = require('../common');
const EventEmitter = require('events');
const assert = require('assert');

const ee = new EventEmitter();
const foo = Symbol('foo');
const listener = common.mustCall(function() {});

ee.on(foo, listener);
assert.deepEqual(ee.listeners(foo), [listener]);

ee.emit(foo);

ee.removeAllListeners();
assert.deepEqual(ee.listeners(foo), []);

ee.on(foo, listener);
assert.deepEqual(ee.listeners(foo), [listener]);

ee.removeListener(foo, listener);
assert.deepEqual(ee.listeners(foo), []);
