process.mixin(require("./common"));
var events = require('events');

function timer (t) {
  var promise = new events.Promise();
  setTimeout(function () {
    promise.emitSuccess();
  }, t);
  return promise;
}

order = 0;
var a = new Date();
function test_timeout_order(delay, desired_order) {
  delay *= 10;
  timer(0).addCallback(function() {
    timer(delay).wait()
    var b = new Date();
    assert.equal(true, b - a >= delay);
    order++;
    // A stronger assertion would be that the ordering is correct.
    // With Poor Man's coroutines we cannot guarentee that.
    // Replacing wait() with actual coroutines would solve that issue.
    // assert.equal(desired_order, order);
  });
}
test_timeout_order(10, 6); // Why does this have the proper order??
test_timeout_order(5, 5);
test_timeout_order(4, 4);
test_timeout_order(3, 3);
test_timeout_order(2, 2);
test_timeout_order(1, 1);
