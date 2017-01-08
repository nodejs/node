'use strict';
const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events').EventEmitter;
const util = require('util');

util.inherits(MyEE, EventEmitter);

function MyEE(cb) {
  this.once(1, cb);
  this.emit(1);
  this.removeAllListeners();
  EventEmitter.call(this);
}

const myee = new MyEE(common.mustCall(function() {}));


util.inherits(ErrorEE, EventEmitter);
function ErrorEE() {
  this.emit('error', new Error('blerg'));
}

assert.throws(function() {
  new ErrorEE();
}, /blerg/);

process.on('exit', function() {
  assert(!(myee._events instanceof Object));
  assert.deepStrictEqual(Object.keys(myee._events), []);
  console.log('ok');
});


function MyEE2() {
  EventEmitter.call(this);
}

MyEE2.prototype = new EventEmitter();

const ee1 = new MyEE2();
const ee2 = new MyEE2();

ee1.on('x', function() {});

assert.strictEqual(ee2.listenerCount('x'), 0);
