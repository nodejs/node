var common = require('../common');
var assert = require('assert');
var events = require('events');

var e = new events.EventEmitter();

var events_new_listener_emited = [];
var times_hello_emited = 0;

e.addListener('newListener', function(event, listener) {
  console.log('newListener: ' + event);
  events_new_listener_emited.push(event);
});

e.on('hello', function(a, b) {
  console.log('hello');
  times_hello_emited += 1;
  assert.equal('a', a);
  assert.equal('b', b);
});

console.log('start');

e.emit('hello', 'a', 'b');

process.addListener('exit', function() {
  assert.deepEqual(['hello'], events_new_listener_emited);
  assert.equal(1, times_hello_emited);
});


