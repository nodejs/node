'use strict';
// Flags: --expose-internals
var common = require('../common');
var assert = require('assert');
var EventEmitter = require('events').EventEmitter;
var util = require('util');
const EEEvents = require('internal/symbols').EEEvents;

util.inherits(MyEE, EventEmitter);

function MyEE(cb) {
  this.once(1, cb);
  this.emit(1);
  this.removeAllListeners();
  EventEmitter.call(this);
}

var called = false;
var myee = new MyEE(function() {
  called = true;
});


util.inherits(ErrorEE, EventEmitter);
function ErrorEE() {
  this.emit('error', new Error('blerg'));
}

assert.throws(function() {
  new ErrorEE();
}, /blerg/);

process.on('exit', function() {
  assert(called);
  assert.strictEqual(myee[EEEvents].size, 0);
  console.log('ok');
});


function MyEE2() {
  EventEmitter.call(this);
}

MyEE2.prototype = new EventEmitter();

var ee1 = new MyEE2();
var ee2 = new MyEE2();

ee1.on('x', function() {});

assert.equal(ee2.listenerCount('x'), 0);
