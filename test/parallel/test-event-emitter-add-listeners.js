'use strict';
require('../common');
var assert = require('assert');
var events = require('events');

var e = new events.EventEmitter();

var events_new_listener_emited = [];
var listeners_new_listener_emited = [];
var times_hello_emited = 0;

// sanity check
assert.equal(e.addListener, e.on);

e.on('newListener', function(event, listener) {
  if (event === 'newListener')
    return; // Don't track our adding of newListener listeners.
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
e.once('newListener', function(name, listener) {
  assert.equal(name, 'hello');
  assert.equal(listener, hello);
  assert.deepStrictEqual(this.listeners('hello'), []);
});
e.on('hello', hello);

var foo = function() {};
e.once('foo', foo);

console.log('start');

e.emit('hello', 'a', 'b');


// just make sure that this doesn't throw:
var f = new events.EventEmitter();
f.setMaxListeners(0);


process.on('exit', function() {
  assert.deepStrictEqual(['hello', 'foo'], events_new_listener_emited);
  assert.deepStrictEqual([hello, foo], listeners_new_listener_emited);
  assert.equal(1, times_hello_emited);
});

var listen1 = function listen1() {};
var listen2 = function listen2() {};
var e1 = new events.EventEmitter();
e1.once('newListener', function() {
  assert.deepStrictEqual(e1.listeners('hello'), []);
  e1.once('newListener', function() {
    assert.deepStrictEqual(e1.listeners('hello'), []);
  });
  e1.on('hello', listen2);
});
e1.on('hello', listen1);
// The order of listeners on an event is not always the order in which the
// listeners were added.
assert.deepStrictEqual(e1.listeners('hello'), [listen2, listen1]);
