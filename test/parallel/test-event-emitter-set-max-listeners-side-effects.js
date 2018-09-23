'use strict';
var common = require('../common');
var assert = require('assert');
var events = require('events');

var e = new events.EventEmitter();

assert.deepEqual(e._events, {});
e.setMaxListeners(5);
assert.deepEqual(e._events, {});
