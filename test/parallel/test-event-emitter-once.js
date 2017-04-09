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

function remove() {
  assert.fail('once->foo should not be emitted');
}

e.once('foo', remove);
e.removeListener('foo', remove);
e.emit('foo');

e.once('e', common.mustCall(function() {
  e.emit('e');
}));

e.once('e', common.mustCall());

e.emit('e');

// Verify that the listener must be a function
assert.throws(() => {
  const ee = new EventEmitter();

  ee.once('foo', null);
}, /^TypeError: "listener" argument must be a function$/);

{
  // once() has different code paths based on the number of arguments being
  // emitted. Verify that all of the cases are covered.
  const maxArgs = 4;

  for (let i = 0; i <= maxArgs; ++i) {
    const ee = new EventEmitter();
    const args = ['foo'];

    for (let j = 0; j < i; ++j)
      args.push(j);

    ee.once('foo', common.mustCall((...params) => {
      assert.deepStrictEqual(params, args.slice(1));
    }));

    EventEmitter.prototype.emit.apply(ee, args);
  }
}
