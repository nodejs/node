include("mjsunit.js");

var p1_done = false;
var p1 = new node.Promise();
p1.addCallback(function () {
  p1_done = true;
});

var p2_done = false;
var p2 = new node.Promise();
p2.addCallback(function () {
  p2_done = true;
  setTimeout(function () {
    p1.emitSuccess();
  }, 100);
});

var p3_done = false;
var p3 = new node.Promise();
p3.addCallback(function () {
  p3_done = true;
});

function onLoad () {

  p2.emitSuccess();

  assertFalse(p1_done);
  assertTrue(p2_done);
  assertFalse(p3_done);

  p1.block()

  assertTrue(p1_done);
  assertTrue(p2_done);
  assertFalse(p3_done);

  p3.emitSuccess();
}


function onExit() {
  assertTrue(p1_done);
  assertTrue(p2_done);
  assertTrue(p3_done);
}
