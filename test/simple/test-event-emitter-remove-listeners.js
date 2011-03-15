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

var e1 = new events.EventEmitter();
e1.addListener('hello', listener1);
e1.removeListener('hello', listener1);
assert.deepEqual([], e1.listeners('hello'));

var e2 = new events.EventEmitter();
e2.addListener('hello', listener1);
e2.removeListener('hello', listener2);
assert.deepEqual([listener1], e2.listeners('hello'));

var e3 = new events.EventEmitter();
e3.addListener('hello', listener1);
e3.addListener('hello', listener2);
e3.removeListener('hello', listener1);
assert.deepEqual([listener2], e3.listeners('hello'));



