'use strict';
const common = require('../common');
const assert = require('assert');
const events = require('events');

var E = events.EventEmitter.prototype;
assert.equal(E.constructor.name, 'EventEmitter');
assert.equal(E.on, E.addListener);  // Same method.
Object.getOwnPropertyNames(E).forEach(function(name) {
  if (name === 'constructor' || name === 'on') return;
  if (typeof E[name] !== 'function') return;
  assert.equal(E[name].name, name);
});
