node.mixin(require("common.js"));

var cancelFired = false;

var promise = new node.Promise();
promise.addCallback(function() {
  assertUnreachable('addCallback should not fire after promise.cancel()');
});

promise.addErrback(function() {
  assertUnreachable('addErrback should not fire after promise.cancel()');
});

promise.addCancelback(function() {
  cancelFired = true;
});

promise.cancel();

setTimeout(function() {
  promise.emitSuccess();
  promise.emitError();
}, 100);

process.addListener('exit', function() {
  assertTrue(cancelFired);
});