'use strict';
require('../common');
const EventEmitter = require('events');
const assert = require('assert');

const EE = new EventEmitter();

assert.throws(() => {
  EE.emit('error', 'Accepts a string');
}, /^Error: Unhandled "error" event\. \(Accepts a string\)$/);

assert.throws(() => {
  EE.emit('error', {message: 'Error!'});
}, /^Error: Unhandled "error" event\. \(\[object Object\]\)$/);
