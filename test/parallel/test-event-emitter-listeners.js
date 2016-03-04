'use strict';

require('../common');
var assert = require('assert');
var events = require('events');

function listener() {}
function listener2() {}

var e1 = new events.EventEmitter();
e1.on('foo', listener);
var fooListeners = e1.listeners('foo');
assert.deepEqual(e1.listeners('foo'), [listener]);
e1.removeAllListeners('foo');
assert.deepEqual(e1.listeners('foo'), []);
assert.deepEqual(fooListeners, [listener]);

var e2 = new events.EventEmitter();
e2.on('foo', listener);
var e2ListenersCopy = e2.listeners('foo');
assert.deepEqual(e2ListenersCopy, [listener]);
assert.deepEqual(e2.listeners('foo'), [listener]);
e2ListenersCopy.push(listener2);
assert.deepEqual(e2.listeners('foo'), [listener]);
assert.deepEqual(e2ListenersCopy, [listener, listener2]);

var e3 = new events.EventEmitter();
e3.on('foo', listener);
var e3ListenersCopy = e3.listeners('foo');
e3.on('foo', listener2);
assert.deepEqual(e3.listeners('foo'), [listener, listener2]);
assert.deepEqual(e3ListenersCopy, [listener]);

var e4 = new events.EventEmitter();
e4.once('foo', listener);
assert.deepEqual(e4.listeners('foo'), [listener]);

var e5 = new events.EventEmitter();
e5.on('foo', listener);
e5.once('foo', listener2);
assert.deepEqual(e5.listeners('foo'), [listener, listener2]);
