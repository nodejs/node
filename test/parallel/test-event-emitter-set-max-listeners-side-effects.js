'use strict';
require('../common');
var assert = require('assert');
var events = require('events');

var e = new events.EventEmitter();

assert(!(e._events instanceof Object));
assert.deepStrictEqual(Object.keys(e._events), []);
e.setMaxListeners(5);
assert.deepStrictEqual(Object.keys(e._events), []);
