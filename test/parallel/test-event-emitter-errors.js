'use strict';
require('../common');
var EventEmitter = require('events');
var assert = require('assert');

var EE = new EventEmitter();

assert.throws(function() {
  EE.emit('error', 'Accepts a string');
}, /Accepts a string/);
