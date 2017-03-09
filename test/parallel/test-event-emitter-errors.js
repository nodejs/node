'use strict';
require('../common');
const EventEmitter = require('events');
const assert = require('assert');

const EE = new EventEmitter();

assert.throws(function() {
  EE.emit('error', 'Accepts a string');
}, /Accepts a string/);
