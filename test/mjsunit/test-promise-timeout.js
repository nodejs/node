process.mixin(require("./common"));

var timeouts = 0;

var promise = new process.Promise();
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

var waitPromise = new process.Promise();
try {
  waitPromise.timeout(250).wait()
} catch (e) {
  assertInstanceof(e, Error);
  assertEquals('timeout', e.message);
  timeouts++;
}

var successPromise = new process.Promise();
successPromise.timeout(500);
setTimeout(function() {
  successPromise.emitSuccess();
}, 250);

successPromise.addErrback(function() {
  assertUnreachable('addErrback should not fire if there is no timeout');
});

var errorPromise = new process.Promise();
errorPromise.timeout(500);
setTimeout(function() {
  errorPromise.emitError(new Error('intentional'));
}, 250);

errorPromise.addErrback(function(e) {
  assertInstanceof(e, Error);
  assertEquals('intentional', e.message);
});

var cancelPromise = new process.Promise();
cancelPromise.timeout(500);
setTimeout(function() {
  cancelPromise.cancel();
}, 250);

setTimeout(function() {
  cancelPromise.emitSuccess('should be ignored');
}, 400);

cancelPromise.addCallback(function(e) {
  assertUnreachable('addCallback should not fire if the promise is canceled');
});

cancelPromise.addErrback(function(e) {
  assertUnreachable('addErrback should not fire if the promise is canceled');
});

var cancelTimeoutPromise = new process.Promise();
cancelTimeoutPromise.timeout(500);
setTimeout(function() {
  cancelPromise.emitCancel();
}, 250);

cancelPromise.addErrback(function(e) {
  assertUnreachable('addErrback should not fire after a cancel event');
});

process.addListener('exit', function() {
  assertEquals(2, timeouts);
});