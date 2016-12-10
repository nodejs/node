'use strict';

require('../common');
const assert = require('assert');
const events = require('events');
const util = require('util');

function listener() {}
function listener2() {}
class TestStream { constructor() { } }
util.inherits(TestStream, events.EventEmitter);

{
  const ee = new events.EventEmitter();
  ee.on('foo', listener);
  const fooListeners = ee.listeners('foo');
  assert.deepStrictEqual(ee.listeners('foo'), [listener]);
  ee.removeAllListeners('foo');
  assert.deepStrictEqual(ee.listeners('foo'), []);
  assert.deepStrictEqual(fooListeners, [listener]);
}

{
  const ee = new events.EventEmitter();
  ee.on('foo', listener);
  const eeListenersCopy = ee.listeners('foo');
  assert.deepStrictEqual(eeListenersCopy, [listener]);
  assert.deepStrictEqual(ee.listeners('foo'), [listener]);
  eeListenersCopy.push(listener2);
  assert.deepStrictEqual(ee.listeners('foo'), [listener]);
  assert.deepStrictEqual(eeListenersCopy, [listener, listener2]);
}

{
  const ee = new events.EventEmitter();
  ee.on('foo', listener);
  const eeListenersCopy = ee.listeners('foo');
  ee.on('foo', listener2);
  assert.deepStrictEqual(ee.listeners('foo'), [listener, listener2]);
  assert.deepStrictEqual(eeListenersCopy, [listener]);
}

{
  const ee = new events.EventEmitter();
  ee.once('foo', listener);
  assert.deepStrictEqual(ee.listeners('foo'), [listener]);
}

{
  const ee = new events.EventEmitter();
  ee.on('foo', listener);
  ee.once('foo', listener2);
  assert.deepStrictEqual(ee.listeners('foo'), [listener, listener2]);
}

{
  const ee = new events.EventEmitter();
  ee._events = undefined;
  assert.deepStrictEqual(ee.listeners('foo'), []);
}

{
  const s = new TestStream();
  assert.deepStrictEqual(s.listeners('foo'), []);
}
