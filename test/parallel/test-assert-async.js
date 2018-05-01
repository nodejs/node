'use strict';
const common = require('../common');
const assert = require('assert');

// Test assert.rejects() and assert.doesNotReject() by checking their
// expected output and by verifying that they do not work sync

common.crashOnUnhandledRejection();

// Run all tests in parallel and check their outcome at the end.
const promises = [];

// Check `assert.rejects`.
{
  const rejectingFn = async () => assert.fail();
  const errObj = {
    code: 'ERR_ASSERTION',
    name: 'AssertionError [ERR_ASSERTION]',
    message: 'Failed'
  };
  // `assert.rejects` accepts a function or a promise as first argument.
  promises.push(assert.rejects(rejectingFn, errObj));
  promises.push(assert.rejects(rejectingFn(), errObj));
}

{
  const handler = (err) => {
    assert(err instanceof assert.AssertionError,
           `${err.name} is not instance of AssertionError`);
    assert.strictEqual(err.code, 'ERR_ASSERTION');
    assert.strictEqual(err.message,
                       'Missing expected rejection (handler).');
    assert.strictEqual(err.operator, 'rejects');
    assert.ok(!err.stack.includes('at Function.rejects'));
    return true;
  };

  let promise = assert.rejects(async () => {}, handler);
  promises.push(assert.rejects(promise, handler));

  promise = assert.rejects(() => {}, handler);
  promises.push(assert.rejects(promise, handler));

  promise = assert.rejects(Promise.resolve(), handler);
  promises.push(assert.rejects(promise, handler));
}

{
  const THROWN_ERROR = new Error();

  promises.push(assert.rejects(() => {
    throw THROWN_ERROR;
  }, {}).catch(common.mustCall((err) => {
    assert.strictEqual(err, THROWN_ERROR);
  })));
}

promises.push(assert.rejects(
  assert.rejects('fail', {}),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "block" argument must be one of type ' +
             'Function or Promise. Received type string'
  }
));

// Check `assert.doesNotReject`.
{
  // `assert.doesNotReject` accepts a function or a promise as first argument.
  promises.push(assert.doesNotReject(() => {}));
  promises.push(assert.doesNotReject(async () => {}));
  promises.push(assert.doesNotReject(Promise.resolve()));
}

{
  const handler1 = (err) => {
    assert(err instanceof assert.AssertionError,
           `${err.name} is not instance of AssertionError`);
    assert.strictEqual(err.code, 'ERR_ASSERTION');
    assert.strictEqual(err.message, 'Failed');
    return true;
  };
  const handler2 = (err) => {
    assert(err instanceof assert.AssertionError,
           `${err.name} is not instance of AssertionError`);
    assert.strictEqual(err.code, 'ERR_ASSERTION');
    assert.strictEqual(err.message,
                       'Got unwanted rejection.\nActual message: "Failed"');
    assert.strictEqual(err.operator, 'doesNotReject');
    assert.ok(!err.stack.includes('at Function.doesNotReject'));
    return true;
  };

  const rejectingFn = async () => assert.fail();

  let promise = assert.doesNotReject(rejectingFn, handler1);
  promises.push(assert.rejects(promise, handler2));

  promise = assert.doesNotReject(rejectingFn(), handler1);
  promises.push(assert.rejects(promise, handler2));

  promise = assert.doesNotReject(() => assert.fail(), common.mustNotCall());
  promises.push(assert.rejects(promise, handler1));
}

promises.push(assert.rejects(
  assert.doesNotReject(123),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "block" argument must be one of type ' +
             'Function or Promise. Received type number'
  }
));

// Make sure all async code gets properly executed.
Promise.all(promises).then(common.mustCall());
