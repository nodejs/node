var common = require('../common');
var assert = require('assert');
var events = require('events');

var e = new events.EventEmitter();
var times_hello_emited = 0;

e.once('hello', function(a, b) {
  times_hello_emited++;
});

e.emit('hello', 'a', 'b');
e.emit('hello', 'a', 'b');
e.emit('hello', 'a', 'b');
e.emit('hello', 'a', 'b');

process.addListener('exit', function() {
  assert.equal(1, times_hello_emited);
});

