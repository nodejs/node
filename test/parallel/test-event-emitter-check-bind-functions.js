'use strict';
require('../common');
const assert = require('assert');
const events = require('events');

{
  const E = events.EventEmitter.prototype;

  assert.strictEqual(E.on, E.addListener);
  assert.strictEqual(E.off, E.removeListener);

  E.on = function() {};
  assert.strictEqual(E.on, E.addListener);

  E.addListener = function() {};
  assert.strictEqual(E.on, E.addListener);

  E.off = function() {};
  assert.strictEqual(E.off, E.removeListener);

  E.removeListener = function() {};
  assert.strictEqual(E.off, E.removeListener);
}

{
  const EventEmitter = events.EventEmitter;

  const ee = new EventEmitter();

  assert.strictEqual(ee.on, ee.addListener);

  ee.on = function() {};
  assert.strictEqual(ee.on, ee.addListener);

  ee.addListener = function() {};
  assert.strictEqual(ee.on, ee.addListener);

  ee.off = function() {};
  assert.strictEqual(ee.off, ee.removeListener);

  ee.removeListener = function() {};
  assert.strictEqual(ee.off, ee.removeListener);
}
