node.mixin(require("common.js"));

var timeouts = 0;

var promise = new node.Promise();
promise.timeout(250);
assertEquals(250, promise.timeout());

promise.addCallback(function() {
  assertUnreachable('addCallback should not fire after a promise error');
});

promise.addErrback(function(e) {
  assertInstanceof(e, Error);
  assertEquals('timeout', e.message);
  timeouts++;
});

setTimeout(function() {
  promise.emitSuccess('Am I too late?');
}, 500);

var waitPromise = new node.Promise();
try {
  waitPromise.timeout(250).wait()
} catch (e) {
  assertInstanceof(e, Error);
  assertEquals('timeout', e.message);
  timeouts++;
}

process.addListener('exit', function() {
  assertEquals(2, timeouts);
});