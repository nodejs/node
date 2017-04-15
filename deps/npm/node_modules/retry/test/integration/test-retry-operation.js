var common = require('../common');
var assert = common.assert;
var fake = common.fake.create();
var retry = require(common.dir.lib + '/retry');

(function testErrors() {
  var operation = retry.operation();

  var error = new Error('some error');
  var error2 = new Error('some other error');
  operation._errors.push(error);
  operation._errors.push(error2);

  assert.deepEqual(operation.errors(), [error, error2]);
})();

(function testMainErrorReturnsMostFrequentError() {
  var operation = retry.operation();
  var error = new Error('some error');
  var error2 = new Error('some other error');

  operation._errors.push(error);
  operation._errors.push(error2);
  operation._errors.push(error);

  assert.strictEqual(operation.mainError(), error);
})();

(function testMainErrorReturnsLastErrorOnEqualCount() {
  var operation = retry.operation();
  var error = new Error('some error');
  var error2 = new Error('some other error');

  operation._errors.push(error);
  operation._errors.push(error2);

  assert.strictEqual(operation.mainError(), error2);
})();

(function testAttempt() {
  var operation = retry.operation();
  var fn = new Function();

  var timeoutOpts = {
    timeout: 1,
    cb: function() {}
  };
  operation.attempt(fn, timeoutOpts);

  assert.strictEqual(fn, operation._fn);
  assert.strictEqual(timeoutOpts.timeout, operation._operationTimeout);
  assert.strictEqual(timeoutOpts.cb, operation._operationTimeoutCb);
})();

(function testRetry() {
  var times = 3;
  var error = new Error('some error');
  var operation = retry.operation([1, 2, 3]);
  var attempts = 0;

  var finalCallback = fake.callback('finalCallback');
  fake.expectAnytime(finalCallback);

  var fn = function() {
    operation.attempt(function(currentAttempt) {
      attempts++;
      assert.equal(currentAttempt, attempts);
      if (operation.retry(error)) {
        return;
      }

      assert.strictEqual(attempts, 4);
      assert.strictEqual(operation.attempts(), attempts);
      assert.strictEqual(operation.mainError(), error);
      finalCallback();
    });
  };

  fn();
})();

(function testRetryForever() {
  var error = new Error('some error');
  var operation = retry.operation({ retries: 3, forever: true });
  var attempts = 0;

  var finalCallback = fake.callback('finalCallback');
  fake.expectAnytime(finalCallback);

  var fn = function() {
    operation.attempt(function(currentAttempt) {
      attempts++;
      assert.equal(currentAttempt, attempts);
      if (attempts !== 6 && operation.retry(error)) {
        return;
      }

      assert.strictEqual(attempts, 6);
      assert.strictEqual(operation.attempts(), attempts);
      assert.strictEqual(operation.mainError(), error);
      finalCallback();
    });
  };

  fn();
})();

(function testRetryForeverNoRetries() {
  var error = new Error('some error');
  var delay = 50
  var operation = retry.operation({
    retries: null,
    forever: true,
    minTimeout: delay,
    maxTimeout: delay
  });

  var attempts = 0;
  var startTime = new Date().getTime();

  var finalCallback = fake.callback('finalCallback');
  fake.expectAnytime(finalCallback);

  var fn = function() {
    operation.attempt(function(currentAttempt) {
      attempts++;
      assert.equal(currentAttempt, attempts);
      if (attempts !== 4 && operation.retry(error)) {
        return;
      }

      var endTime = new Date().getTime();
      var minTime = startTime + (delay * 3);
      var maxTime = minTime + 20 // add a little headroom for code execution time
      assert(endTime > minTime)
      assert(endTime < maxTime)
      assert.strictEqual(attempts, 4);
      assert.strictEqual(operation.attempts(), attempts);
      assert.strictEqual(operation.mainError(), error);
      finalCallback();
    });
  };

  fn();
})();

(function testStop() {
  var error = new Error('some error');
  var operation = retry.operation([1, 2, 3]);
  var attempts = 0;

  var finalCallback = fake.callback('finalCallback');
  fake.expectAnytime(finalCallback);

  var fn = function() {
    operation.attempt(function(currentAttempt) {
      attempts++;
      assert.equal(currentAttempt, attempts);

      if (attempts === 2) {
        operation.stop();

        assert.strictEqual(attempts, 2);
        assert.strictEqual(operation.attempts(), attempts);
        assert.strictEqual(operation.mainError(), error);
        finalCallback();
      }

      if (operation.retry(error)) {
        return;
      }
    });
  };

  fn();
})();
