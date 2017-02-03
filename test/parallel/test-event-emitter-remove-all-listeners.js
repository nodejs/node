'use strict';
const common = require('../common');
const assert = require('assert');
const events = require('events');


function expect(expected) {
  const actual = [];
  process.on('exit', function() {
    assert.deepStrictEqual(actual.sort(), expected.sort());
  });
  function listener(name) {
    actual.push(name);
  }
  return common.mustCall(listener, expected.length);
}

function listener() {}

{
  const ee = new events.EventEmitter();
  ee.on('foo', listener);
  ee.on('bar', listener);
  ee.on('baz', listener);
  ee.on('baz', listener);
  const fooListeners = ee.listeners('foo');
  const barListeners = ee.listeners('bar');
  const bazListeners = ee.listeners('baz');
  ee.on('removeListener', expect(['bar', 'baz', 'baz']));
  ee.removeAllListeners('bar');
  ee.removeAllListeners('baz');
  assert.deepStrictEqual(ee.listeners('foo'), [listener]);
  assert.deepStrictEqual(ee.listeners('bar'), []);
  assert.deepStrictEqual(ee.listeners('baz'), []);
  // After calling removeAllListeners(),
  // the old listeners array should stay unchanged.
  assert.deepStrictEqual(fooListeners, [listener]);
  assert.deepStrictEqual(barListeners, [listener]);
  assert.deepStrictEqual(bazListeners, [listener, listener]);
  // After calling removeAllListeners(),
  // new listeners arrays is different from the old.
  assert.notStrictEqual(ee.listeners('bar'), barListeners);
  assert.notStrictEqual(ee.listeners('baz'), bazListeners);
}

{
  const ee = new events.EventEmitter();
  ee.on('foo', listener);
  ee.on('bar', listener);
  // Expect LIFO order
  ee.on('removeListener', expect(['foo', 'bar', 'removeListener']));
  ee.on('removeListener', expect(['foo', 'bar']));
  ee.removeAllListeners();
  assert.deepStrictEqual([], ee.listeners('foo'));
  assert.deepStrictEqual([], ee.listeners('bar'));
}

{
  const ee = new events.EventEmitter();
  ee.on('removeListener', listener);
  // Check for regression where removeAllListeners() throws when
  // there exists a 'removeListener' listener, but there exists
  // no listeners for the provided event type.
  assert.doesNotThrow(ee.removeAllListeners.bind(ee, 'foo'));
}

{
  const ee = new events.EventEmitter();
  let expectLength = 2;
  ee.on('removeListener', function(name, listener) {
    assert.strictEqual(expectLength--, this.listeners('baz').length);
  });
  ee.on('baz', function() {});
  ee.on('baz', function() {});
  ee.on('baz', function() {});
  assert.strictEqual(ee.listeners('baz').length, expectLength + 1);
  ee.removeAllListeners('baz');
  assert.strictEqual(ee.listeners('baz').length, 0);
}

{
  const ee = new events.EventEmitter();
  assert.deepStrictEqual(ee, ee.removeAllListeners());
}

{
  const ee = new events.EventEmitter();
  ee._events = undefined;
  assert.strictEqual(ee, ee.removeAllListeners());
}
