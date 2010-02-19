process.mixin(require("./common"));
events = require('events');

var timeouts = 0;

var promise = new events.Promise();
promise.timeout(250);
assert.equal(250, promise.timeout());

promise.addCallback(function() {
  assert.ok(false, 'addCallback should not fire after a promise error');
});

promise.addErrback(function(e) {
  assert.equal(true, e instanceof Error);
  assert.equal('timeout', e.message);
  timeouts++;
});

setTimeout(function() {
  promise.emitSuccess('Am I too late?');
}, 500);

var successPromise = new events.Promise();
successPromise.timeout(500);
setTimeout(function() {
  successPromise.emitSuccess();
}, 250);

successPromise.addErrback(function() {
  assert.ok(false, 'addErrback should not fire if there is no timeout');
});

var errorPromise = new events.Promise();
errorPromise.timeout(500);
setTimeout(function() {
  errorPromise.emitError(new Error('intentional'));
}, 250);

errorPromise.addErrback(function(e) {
  assert.equal(true, e instanceof Error);
  assert.equal('intentional', e.message);
});

process.addListener('exit', function() {
  assert.equal(1, timeouts);
});
