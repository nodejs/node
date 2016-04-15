'use strict';

require('../common');
const assert = require('assert');
const events = require('events');

function listener() {}
function listener2() {}

const e1 = new events.EventEmitter();
e1.on('foo', listener);
const fooListeners = e1.listeners('foo');
assert.deepEqual(e1.listeners('foo'), [listener]);
e1.removeAllListeners('foo');
assert.deepEqual(e1.listeners('foo'), []);
assert.deepEqual(fooListeners, [listener]);

const e2 = new events.EventEmitter();
e2.on('foo', listener);
const e2ListenersCopy = e2.listeners('foo');
assert.deepEqual(e2ListenersCopy, [listener]);
assert.deepEqual(e2.listeners('foo'), [listener]);
e2ListenersCopy.push(listener2);
assert.deepEqual(e2.listeners('foo'), [listener]);
assert.deepEqual(e2ListenersCopy, [listener, listener2]);

const e3 = new events.EventEmitter();
e3.on('foo', listener);
const e3ListenersCopy = e3.listeners('foo');
e3.on('foo', listener2);
assert.deepEqual(e3.listeners('foo'), [listener, listener2]);
assert.deepEqual(e3ListenersCopy, [listener]);

const e4 = new events.EventEmitter();
e4.once('foo', listener);
const e4ListenersCopy = e4.listeners('foo');
e4.on('foo', listener2);
assert.deepEqual(e4.listeners('foo'), [listener, listener2]);
assert.deepEqual(e4ListenersCopy, [listener]);
