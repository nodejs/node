'use strict';
var common = require('../common');
var assert = require('assert');
var events = require('events');

var E = events.EventEmitter.prototype;
assert.equal(E.constructor.name, 'EventEmitter');
assert.equal(E.on, E.addListener);  // Same method.
assert.equal(E.un, E.removeListener);  // Same method.
Object.getOwnPropertyNames(E).forEach(function(name) {
  if (['constructor', 'on', 'un'].indexOf(name) !== -1) return;
  if (typeof E[name] !== 'function') return;
  assert.equal(E[name].name, name);
});
