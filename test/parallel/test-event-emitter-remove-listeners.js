'use strict';
var common = require('../common');
var assert = require('assert');
var events = require('events');

var count = 0;

function listener1() {
  console.log('listener1');
  count++;
}

function listener2() {
  console.log('listener2');
  count++;
}

function listener3() {
  console.log('listener3');
  count++;
}

function remove1() {
  assert(0);
}

function remove2() {
  assert(0);
}

var e1 = new events.EventEmitter();
e1.on('hello', listener1);
e1.on('removeListener', common.mustCall(function(name, cb) {
  assert.equal(name, 'hello');
  assert.equal(cb, listener1);
}));
e1.removeListener('hello', listener1);
assert.deepEqual([], e1.listeners('hello'));

var e2 = new events.EventEmitter();
e2.on('hello', listener1);
e2.on('removeListener', common.fail);
e2.removeListener('hello', listener2);
assert.deepEqual([listener1], e2.listeners('hello'));

var e3 = new events.EventEmitter();
e3.on('hello', listener1);
e3.on('hello', listener2);
e3.once('removeListener', common.mustCall(function(name, cb) {
  assert.equal(name, 'hello');
  assert.equal(cb, listener1);
  assert.deepEqual([listener2], e3.listeners('hello'));
}));
e3.removeListener('hello', listener1);
assert.deepEqual([listener2], e3.listeners('hello'));
e3.once('removeListener', common.mustCall(function(name, cb) {
  assert.equal(name, 'hello');
  assert.equal(cb, listener2);
  assert.deepEqual([], e3.listeners('hello'));
}));
e3.removeListener('hello', listener2);
assert.deepEqual([], e3.listeners('hello'));

var e4 = new events.EventEmitter();
e4.on('removeListener', common.mustCall(function(name, cb) {
  if (cb !== remove1) return;
  this.removeListener('quux', remove2);
  this.emit('quux');
}, 2));
e4.on('quux', remove1);
e4.on('quux', remove2);
e4.removeListener('quux', remove1);

var e5 = new events.EventEmitter();
e5.on('hello', listener1);
e5.on('hello', listener2);
e5.once('removeListener', common.mustCall(function(name, cb) {
  assert.equal(name, 'hello');
  assert.equal(cb, listener1);
  assert.deepEqual([listener2], e5.listeners('hello'));
  e5.once('removeListener', common.mustCall(function(name, cb) {
    assert.equal(name, 'hello');
    assert.equal(cb, listener2);
    assert.deepEqual([], e5.listeners('hello'));
  }));
  e5.removeListener('hello', listener2);
  assert.deepEqual([], e5.listeners('hello'));
}));
e5.removeListener('hello', listener1);
assert.deepEqual([], e5.listeners('hello'));
