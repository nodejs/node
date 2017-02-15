'use strict';
const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events');

function listener1() {}
function listener2() {}

{
  const ee = new EventEmitter();
  ee.on('hello', listener1);
  ee.on('removeListener', common.mustCall((name, cb) => {
    assert.strictEqual(name, 'hello');
    assert.strictEqual(cb, listener1);
  }));
  ee.removeListener('hello', listener1);
  assert.deepStrictEqual([], ee.listeners('hello'));
}

{
  const ee = new EventEmitter();
  ee.on('hello', listener1);
  ee.on('removeListener', common.mustNotCall());
  ee.removeListener('hello', listener2);
  assert.deepStrictEqual([listener1], ee.listeners('hello'));
}

{
  const ee = new EventEmitter();
  ee.on('hello', listener1);
  ee.on('hello', listener2);
  ee.once('removeListener', common.mustCall((name, cb) => {
    assert.strictEqual(name, 'hello');
    assert.strictEqual(cb, listener1);
    assert.deepStrictEqual([listener2], ee.listeners('hello'));
  }));
  ee.removeListener('hello', listener1);
  assert.deepStrictEqual([listener2], ee.listeners('hello'));
  ee.once('removeListener', common.mustCall((name, cb) => {
    assert.strictEqual(name, 'hello');
    assert.strictEqual(cb, listener2);
    assert.deepStrictEqual([], ee.listeners('hello'));
  }));
  ee.removeListener('hello', listener2);
  assert.deepStrictEqual([], ee.listeners('hello'));
}

{
  const ee = new EventEmitter();

  function remove1() {
    common.fail('remove1 should not have been called');
  }

  function remove2() {
    common.fail('remove2 should not have been called');
  }

  ee.on('removeListener', common.mustCall(function(name, cb) {
    if (cb !== remove1) return;
    this.removeListener('quux', remove2);
    this.emit('quux');
  }, 2));
  ee.on('quux', remove1);
  ee.on('quux', remove2);
  ee.removeListener('quux', remove1);
}

{
  const ee = new EventEmitter();
  ee.on('hello', listener1);
  ee.on('hello', listener2);
  ee.once('removeListener', common.mustCall((name, cb) => {
    assert.strictEqual(name, 'hello');
    assert.strictEqual(cb, listener1);
    assert.deepStrictEqual([listener2], ee.listeners('hello'));
    ee.once('removeListener', common.mustCall((name, cb) => {
      assert.strictEqual(name, 'hello');
      assert.strictEqual(cb, listener2);
      assert.deepStrictEqual([], ee.listeners('hello'));
    }));
    ee.removeListener('hello', listener2);
    assert.deepStrictEqual([], ee.listeners('hello'));
  }));
  ee.removeListener('hello', listener1);
  assert.deepStrictEqual([], ee.listeners('hello'));
}

{
  const ee = new EventEmitter();
  const listener3 = common.mustCall(() => {
    ee.removeListener('hello', listener4);
  }, 2);
  const listener4 = common.mustCall(() => {});

  ee.on('hello', listener3);
  ee.on('hello', listener4);

  // listener4 will still be called although it is removed by listener 3.
  ee.emit('hello');
  // This is so because the interal listener array at time of emit
  // was [listener3,listener4]

  // Interal listener array [listener3]
  ee.emit('hello');
}

{
  const ee = new EventEmitter();

  ee.once('hello', listener1);
  ee.on('removeListener', common.mustCall((eventName, listener) => {
    assert.strictEqual(eventName, 'hello');
    assert.strictEqual(listener, listener1);
  }));
  ee.emit('hello');
}

{
  const ee = new EventEmitter();

  assert.deepStrictEqual(ee, ee.removeListener('foo', () => {}));
}

// Verify that the removed listener must be a function
assert.throws(() => {
  const ee = new EventEmitter();

  ee.removeListener('foo', null);
}, /^TypeError: "listener" argument must be a function$/);

{
  const ee = new EventEmitter();
  const listener = () => {};
  ee._events = undefined;
  const e = ee.removeListener('foo', listener);
  assert.strictEqual(e, ee);
}
