'use strict';
const EventEmitter = require('events');
const assert = require('assert');

var EE = new EventEmitter();

assert.throws(function() {
  EE.emit('error', 'Accepts a string');
}, /Accepts a string/);
