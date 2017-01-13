'use strict';
const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events');

{
  const ee = new EventEmitter();
  const events_new_listener_emited = [];
  const listeners_new_listener_emited = [];

  // Sanity check
  assert.strictEqual(ee.addListener, ee.on);

  ee.on('newListener', function(event, listener) {
    // Don't track newListener listeners.
    if (event === 'newListener')
      return;

    events_new_listener_emited.push(event);
    listeners_new_listener_emited.push(listener);
  });

  const hello = common.mustCall(function(a, b) {
    assert.strictEqual('a', a);
    assert.strictEqual('b', b);
  });

  ee.once('newListener', function(name, listener) {
    assert.strictEqual(name, 'hello');
    assert.strictEqual(listener, hello);
    assert.deepStrictEqual(this.listeners('hello'), []);
  });

  ee.on('hello', hello);
  ee.once('foo', common.fail);
  assert.deepStrictEqual(['hello', 'foo'], events_new_listener_emited);
  assert.deepStrictEqual([hello, common.fail], listeners_new_listener_emited);

  ee.emit('hello', 'a', 'b');
}

// just make sure that this doesn't throw:
{
  const f = new EventEmitter();

  f.setMaxListeners(0);
}

{
  const listen1 = function listen1() {};
  const listen2 = function listen2() {};
  const ee = new EventEmitter();

  ee.once('newListener', function() {
    assert.deepStrictEqual(ee.listeners('hello'), []);
    ee.once('newListener', function() {
      assert.deepStrictEqual(ee.listeners('hello'), []);
    });
    ee.on('hello', listen2);
  });
  ee.on('hello', listen1);
  // The order of listeners on an event is not always the order in which the
  // listeners were added.
  assert.deepStrictEqual(ee.listeners('hello'), [listen2, listen1]);
}

// Verify that the listener must be a function
assert.throws(() => {
  const ee = new EventEmitter();

  ee.on('foo', null);
}, /^TypeError: "listener" argument must be a function$/);
