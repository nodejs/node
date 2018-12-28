// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events').EventEmitter;

Object.setPrototypeOf(MyEE.prototype, EventEmitter.prototype);
Object.setPrototypeOf(MyEE, EventEmitter);

function MyEE(cb) {
  this.once(1, cb);
  this.emit(1);
  this.removeAllListeners();
  EventEmitter.call(this);
}

const myee = new MyEE(common.mustCall());

myee.hasOwnProperty('usingDomains');

Object.setPrototypeOf(ErrorEE.prototype, EventEmitter.prototype);
Object.setPrototypeOf(ErrorEE, EventEmitter);
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

ee1.on('x', () => {});

assert.strictEqual(ee2.listenerCount('x'), 0);
