process.mixin(require("./common"));

var e = new process.EventEmitter();

var events_new_listener_emited = [];
var times_hello_emited = 0;

e.addListener("newListener", function (event, listener) {
  puts("newListener: " + event);
  events_new_listener_emited.push(event);
});

e.addListener("hello", function (a, b) {
  puts("hello");
  times_hello_emited += 1
  assertEquals("a", a);
  assertEquals("b", b);
});

puts("start");

e.emit("hello", "a", "b");

process.addListener("exit", function () {
  assertArrayEquals(["hello"], events_new_listener_emited);
  assertEquals(1, times_hello_emited);
});


