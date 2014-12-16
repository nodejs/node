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

var e = new events.EventEmitter();

var events_new_listener_emited = [];
var listeners_new_listener_emited = [];
var times_hello_emited = 0;

// sanity check
assert.equal(e.addListener, e.on);

e.on('newListener', function(event, listener) {
  console.log('newListener: ' + event);
  events_new_listener_emited.push(event);
  listeners_new_listener_emited.push(listener);
});

function hello(a, b) {
  console.log('hello');
  times_hello_emited += 1;
  assert.equal('a', a);
  assert.equal('b', b);
}
e.on('hello', hello);

var foo = function() {};
e.once('foo', foo);

console.log('start');

e.emit('hello', 'a', 'b');


// just make sure that this doesn't throw:
var f = new events.EventEmitter();
f.setMaxListeners(0);


process.on('exit', function() {
  assert.deepEqual(['hello', 'foo'], events_new_listener_emited);
  assert.deepEqual([hello, foo], listeners_new_listener_emited);
  assert.equal(1, times_hello_emited);
});


