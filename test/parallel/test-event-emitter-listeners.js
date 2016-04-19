'use strict';

require('../common');
var assert = require('assert');
var events = require('events');

function listener() {}
function listener2() {}

var e1 = new events.EventEmitter();
e1.on('foo', listener);
var fooListeners = e1.listeners('foo');
assert.deepStrictEqual(e1.listeners('foo'), [listener]);
e1.removeAllListeners('foo');
assert.deepStrictEqual(e1.listeners('foo'), []);
assert.deepStrictEqual(fooListeners, [listener]);

var e2 = new events.EventEmitter();
e2.on('foo', listener);
var e2ListenersCopy = e2.listeners('foo');
assert.deepStrictEqual(e2ListenersCopy, [listener]);
assert.deepStrictEqual(e2.listeners('foo'), [listener]);
e2ListenersCopy.push(listener2);
assert.deepStrictEqual(e2.listeners('foo'), [listener]);
assert.deepStrictEqual(e2ListenersCopy, [listener, listener2]);

var e3 = new events.EventEmitter();
e3.on('foo', listener);
var e3ListenersCopy = e3.listeners('foo');
e3.on('foo', listener2);
assert.deepStrictEqual(e3.listeners('foo'), [listener, listener2]);
assert.deepStrictEqual(e3ListenersCopy, [listener]);
