'use strict';
// Flags: --expose-internals
var common = require('../common');
var assert = require('assert');
var events = require('events');
const EEEvents = require('internal/symbols').EEEvents;

var e = new events.EventEmitter();

assert.strictEqual(e[EEEvents].size, 0);
e.setMaxListeners(5);
assert.strictEqual(e[EEEvents].size, 0);
