'use strict';
const common = require('../common');
const assert = require('assert');
const events = require('events');

var e = new events.EventEmitter();

assert.deepEqual(e._events, {});
e.setMaxListeners(5);
assert.deepEqual(e._events, {});
