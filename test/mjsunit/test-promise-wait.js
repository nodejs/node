process.mixin(require("./common"));
events = require('events');

var p1_done = false;
var p1 = new events.Promise();
p1.addCallback(function () {
  assert.equal(1, arguments.length);
  assert.equal("single arg", arguments[0]);
  p1_done = true;
});

var p2_done = false;
var p2 = new events.Promise();
p2.addCallback(function () {
  p2_done = true;
  setTimeout(function () {
    p1.emitSuccess("single arg");
  }, 100);
});

var p3_done = false;
var p3 = new events.Promise();
p3.addCallback(function () {
  p3_done = true;
});



var p4_done = false;
var p4 = new events.Promise();
p4.addCallback(function () {
  assert.equal(3, arguments.length);
  assert.equal("a", arguments[0]);
  assert.equal("b", arguments[1]);
  assert.equal("c", arguments[2]);
  p4_done = true;
});

var p5_done = false;
var p5 = new events.Promise();
p5.addCallback(function () {
  p5_done = true;
  setTimeout(function () {
    p4.emitSuccess("a","b","c");
  }, 100);
});

var p6 = new events.Promise();
var p7 = new events.Promise();
p7.addErrback(function() {});

p2.emitSuccess();

assert.equal(false, p1_done);
assert.equal(true, p2_done);
assert.equal(false, p3_done);

var ret1 = p1.wait()
assert.equal("single arg", ret1);

assert.equal(true, p1_done);
assert.equal(true, p2_done);
assert.equal(false, p3_done);

p3.emitSuccess();

assert.equal(false, p4_done);
assert.equal(false, p5_done);

p5.emitSuccess();

assert.equal(false, p4_done);
assert.equal(true, p5_done);

var ret4 = p4.wait();
assert.deepEqual(["a","b","c"], ret4);

assert.equal(true, p4_done);


p6.emitSuccess("something");
assert.equal("something", p6.wait());
p7.emitError("argh!");
var goterr;
try {
	p7.wait();
} catch(err) {
	goterr = err;
}
assert.equal("argh!", goterr.toString());

process.addListener("exit", function () {
  assert.equal(true, p1_done);
  assert.equal(true, p2_done);
  assert.equal(true, p3_done);
  assert.equal(true, p4_done);
  assert.equal(true, p5_done);
});
