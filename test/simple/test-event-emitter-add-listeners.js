require("../common");
var events = require('events');

var e = new events.EventEmitter();

var events_new_listener_emited = [];
var times_hello_emited = 0;

e.addListener("newListener", function (event, listener) {
  puts("newListener: " + event);
  events_new_listener_emited.push(event);
});

e.addListener("hello", function (a, b) {
  puts("hello");
  times_hello_emited += 1
  assert.equal("a", a);
  assert.equal("b", b);
});

puts("start");

e.emit("hello", "a", "b");

process.addListener("exit", function () {
  assert.deepEqual(["hello"], events_new_listener_emited);
  assert.equal(1, times_hello_emited);
});


