'use strict';
require('../common');
const assert = require('assert');
const events = require('events');

const E = events.EventEmitter.prototype;
assert.strictEqual(E.constructor.name, 'EventEmitter');
assert.strictEqual(E.on, E.addListener);  // Same method.
Object.getOwnPropertyNames(E).forEach(function(name) {
  if (name === 'constructor' || name === 'on') return;
  if (typeof E[name] !== 'function') return;
  assert.strictEqual(E[name].name, name);
});
