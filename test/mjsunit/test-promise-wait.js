process.mixin(require("./common"));

var p1_done = false;
var p1 = new process.Promise();
p1.addCallback(function () {
  assertEquals(1, arguments.length);
  assertEquals("single arg", arguments[0]);
  p1_done = true;
});

var p2_done = false;
var p2 = new process.Promise();
p2.addCallback(function () {
  p2_done = true;
  setTimeout(function () {
    p1.emitSuccess("single arg");
  }, 100);
});

var p3_done = false;
var p3 = new process.Promise();
p3.addCallback(function () {
  p3_done = true;
});



var p4_done = false;
var p4 = new process.Promise();
p4.addCallback(function () {
  assertEquals(3, arguments.length);
  assertEquals("a", arguments[0]);
  assertEquals("b", arguments[1]);
  assertEquals("c", arguments[2]);
  p4_done = true;
});

var p5_done = false;
var p5 = new process.Promise();
p5.addCallback(function () {
  p5_done = true;
  setTimeout(function () {
    p4.emitSuccess("a","b","c");
  }, 100);
});


p2.emitSuccess();

assertFalse(p1_done);
assertTrue(p2_done);
assertFalse(p3_done);

var ret1 = p1.wait()
assertEquals("single arg", ret1);

assertTrue(p1_done);
assertTrue(p2_done);
assertFalse(p3_done);

p3.emitSuccess();

assertFalse(p4_done);
assertFalse(p5_done);

p5.emitSuccess();

assertFalse(p4_done);
assertTrue(p5_done);

var ret4 = p4.wait();
assertArrayEquals(["a","b","c"], ret4);

assertTrue(p4_done);

process.addListener("exit", function () {
  assertTrue(p1_done);
  assertTrue(p2_done);
  assertTrue(p3_done);
  assertTrue(p4_done);
  assertTrue(p5_done);
});
