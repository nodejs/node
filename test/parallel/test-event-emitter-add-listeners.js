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


