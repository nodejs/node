var assert = require('assert');

var immediateThis, intervalThis, timeoutThis, intervalUnrefThis,
    timeoutUnrefThis, immediateArgsThis, intervalArgsThis, timeoutArgsThis,
    intervalUnrefArgsThis, timeoutUnrefArgsThis;

var immediateHandler = setImmediate(function () {
  immediateThis = this;
});

var immediateArgsHandler = setImmediate(function () {
  immediateArgsThis = this;
}, "args ...");


var intervalHandler = setInterval(function () {
  clearInterval(intervalHandler);

  intervalThis = this;
});

var intervalArgsHandler = setInterval(function () {
  clearInterval(intervalArgsHandler);

  intervalArgsThis = this;
}, 0, "args ...");

var intervalUnrefHandler = setInterval(function () {
  clearInterval(intervalUnrefHandler);

  intervalUnrefThis = this;
});
intervalUnrefHandler.unref();

var intervalUnrefArgsHandler = setInterval(function () {
  clearInterval(intervalUnrefArgsHandler);

  intervalUnrefArgsThis = this;
}, 0, "args ...");
intervalUnrefArgsHandler.unref();


var timeoutHandler = setTimeout(function () {
  timeoutThis = this;
});

var timeoutArgsHandler = setTimeout(function () {
  timeoutArgsThis = this;
}, 0, "args ...");

var timeoutUnrefHandler = setTimeout(function () {
  timeoutUnrefThis = this;
});
timeoutUnrefHandler.unref();

var timeoutUnrefArgsHandler = setTimeout(function () {
  timeoutUnrefArgsThis = this;
}, 0, "args ...");
timeoutUnrefArgsHandler.unref();

setTimeout(function() {}, 5);

process.once('exit', function () {
  assert.strictEqual(immediateThis, immediateHandler);
  assert.strictEqual(immediateArgsThis, immediateArgsHandler);

  assert.strictEqual(intervalThis, intervalHandler);
  assert.strictEqual(intervalArgsThis, intervalArgsHandler);
  assert.strictEqual(intervalUnrefThis, intervalUnrefHandler);
  assert.strictEqual(intervalUnrefArgsThis, intervalUnrefArgsHandler);

  assert.strictEqual(timeoutThis, timeoutHandler);
  assert.strictEqual(timeoutArgsThis, timeoutArgsHandler);
  assert.strictEqual(timeoutUnrefThis, timeoutUnrefHandler);
  assert.strictEqual(timeoutUnrefArgsThis, timeoutUnrefArgsHandler);
});
