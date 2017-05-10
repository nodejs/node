'use strict';
var util = require('util');
var assert = require('assert');
var EventEmitter = require('events').EventEmitter;
var called = false;

function TestClass() {
}
TestClass.prototype = new EventEmitter();

function okListener() {
  called = true;
}
function brokenListener() {
  throw new Error('This one should not be called!');
}

var ok = new TestClass();
var broken = new TestClass();
broken.on('end', okListener);
ok.on('end', brokenListener);
ok.emit('end');
assert.ok(called, 'The ok listener should have been called');
