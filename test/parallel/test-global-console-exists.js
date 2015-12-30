'use strict';

const assert = require('assert');
const events = require('events');

const old_default = events.defaultMaxListeners;
events.defaultMaxListeners = 1;

const e = new events.EventEmitter();
e.on('hello', function() {});

assert.ok(!e._events['hello'].hasOwnProperty('warned'));

e.on('hello', function() {});

assert.ok(e._events['hello'].hasOwnProperty('warned'));

events.defaultMaxListeners = old_default;

// this caches console, so place it after the test just to appease the linter
require('../common');
