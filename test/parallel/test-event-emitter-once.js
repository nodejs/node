'use strict';
const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events');

const e = new EventEmitter();

e.once('hello', common.mustCall(function(a, b) {}));

e.emit('hello', 'a', 'b');
e.emit('hello', 'a', 'b');
e.emit('hello', 'a', 'b');
e.emit('hello', 'a', 'b');

var remove = function() {
  common.fail('once->foo should not be emitted');
};

e.once('foo', remove);
e.removeListener('foo', remove);
e.emit('foo');

e.once('e', common.mustCall(function() {
  e.emit('e');
}));

e.once('e', common.mustCall(function() {}));

e.emit('e');

// Verify that the listener must be a function
assert.throws(() => {
  const ee = new EventEmitter();

  ee.once('foo', null);
}, /^TypeError: "listener" argument must be a function$/);
