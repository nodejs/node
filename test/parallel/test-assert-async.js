'use strict';
const common = require('../common');
const assert = require('assert');
const { describe, it } = require('node:test');

/* eslint-disable no-restricted-syntax */

// Thenable object without `catch` method,
// shouldn't be considered as a valid Thenable
const invalidThenable = {
  then: (fulfill, reject) => {
    fulfill();
  },
};

// Function that returns a Thenable function,
// a function with `catch` and `then` methods attached,
// shouldn't be considered as a valid Thenable.
const invalidThenableFunc = () => {
  function f() {}

  f.then = (fulfill, reject) => {
    fulfill();
  };
  f.catch = () => {};

  return f;
};

const validRejectingThenable = {
  then: (fulfill, reject) => {
    reject({ code: 'FAIL' });
  },
  catch: () => {}
};

describe('assert.rejects()', () => {
  const rejectingFn = async () => assert.fail();
  const errObj = {
    code: 'ERR_ASSERTION',
    name: 'AssertionError',
    message: 'Failed'
  };

  it('assert.rejects() accepts a function, promise, or thenable as first argument', async () => {
    await assert.rejects(rejectingFn, errObj);
    await assert.rejects(rejectingFn(), errObj);
    await assert.rejects(validRejectingThenable, { code: 'FAIL' });
  });

  it('should not accept thenables that use a function as `obj` and that have no `catch` handler', async () => {
    await assert.rejects(assert.rejects(invalidThenable, {}), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  });

  it('The validate validation function is expected to return "true"', async () => {
    const err = new Error('foobar');
    const validate = () => { return 'baz'; };
    await assert.rejects(
      () => assert.rejects(Promise.reject(err), validate),
      {
        message: 'The "validate" validation function is expected to ' +
                 "return \"true\". Received 'baz'\n\nCaught error:\n\n" +
                 'Error: foobar',
        code: 'ERR_ASSERTION',
        actual: err,
        expected: validate,
        name: 'AssertionError',
        operator: 'rejects',
      }
    );
  });

  it('assert.rejects() accepts a function as the second argument', async () => {

    const handler = (err) => {
      assert(err instanceof assert.AssertionError,
             `${err.name} is not instance of AssertionError`);
      assert.strictEqual(err.code, 'ERR_ASSERTION');
      assert.strictEqual(err.message,
                         'Missing expected rejection (mustNotCall).');
      assert.strictEqual(err.operator, 'rejects');
      assert.ok(!err.stack.includes('at Function.rejects'));
      return true;
    };

    await assert.rejects(assert.rejects(async () => {}, common.mustNotCall()),
                         common.mustCall(handler));

    await assert.rejects(assert.rejects(() => {}, common.mustNotCall()), {
      name: 'TypeError',
      code: 'ERR_INVALID_RETURN_VALUE',
      // FIXME(JakobJingleheimer): This should match on key words, like /Promise/ and /undefined/.
      message: 'Expected instance of Promise to be returned ' +
               'from the "promiseFn" function but got undefined.'
    });

    await assert.rejects(assert.rejects(Promise.resolve(), common.mustNotCall()), common.mustCall(handler));

  });

  it('assert.rejects() rejects with the error thrown by the function', async () => {
    const THROWN_ERROR = new Error();

    await assert.rejects(() => {
      throw THROWN_ERROR;
    }, {}).catch(common.mustCall((err) => {
      assert.strictEqual(err, THROWN_ERROR);
    }));
  });

  it('assert.rejects() argument must be a function or a promise', async () => {
    await assert.rejects(assert.rejects('fail', {}), {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "promiseFn" argument must be of type function or an ' +
               "instance of Promise. Received type string ('fail')"
    });
  });

  it('uses generatedMessage appropriate', async () => {
    const handler = (generated, actual, err) => {
      assert.strictEqual(err.generatedMessage, generated);
      assert.strictEqual(err.code, 'ERR_ASSERTION');
      assert.strictEqual(err.actual, actual);
      assert.strictEqual(err.operator, 'rejects');
      assert.match(err.stack, /rejects/);
      return true;
    };
    const err = new Error();
    await assert.rejects(
      assert.rejects(Promise.reject(null), { code: 'FOO' }),
      handler.bind(null, true, null)
    );
    await assert.rejects(
      assert.rejects(Promise.reject(5), { code: 'FOO' }, 'AAAAA'),
      handler.bind(null, false, 5)
    );
    await assert.rejects(
      assert.rejects(Promise.reject(err), { code: 'FOO' }, 'AAAAA'),
      handler.bind(null, false, err)
    );
  });
});

describe('assert.doesNotReject()', async () => {
  it('accepts a function, promise, or thenable as the first argument', async () => {
    await assert.rejects(assert.doesNotReject(() => new Map(), common.mustNotCall()), {
      message: 'Expected instance of Promise to be returned ' +
              'from the "promiseFn" function but got an instance of Map.',
      code: 'ERR_INVALID_RETURN_VALUE',
      name: 'TypeError'
    });
    await assert.doesNotReject(async () => {});
    await assert.doesNotReject(Promise.resolve());
  });

  it('does not accept thenables that use a function as `obj` and that have no `catch` handler',
     async () => {
       // `assert.doesNotReject` should not accept thenables that
       // use a function as `obj` and that have no `catch` handler.
       const validFulfillingThenable = {
         then: (fulfill, reject) => {
           fulfill();
         },
         catch: () => {}
       };
       await assert.doesNotReject(validFulfillingThenable);
       await assert.rejects(
         assert.doesNotReject(invalidThenable),
         {
           code: 'ERR_INVALID_ARG_TYPE'
         });
       await assert.rejects(
         assert.doesNotReject(invalidThenableFunc),
         {
           code: 'ERR_INVALID_RETURN_VALUE'
         });
     });

  it('generally just works as expected', async () => {
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
      assert.ok(err.stack);
      assert.ok(!err.stack.includes('at Function.doesNotReject'));
      return true;
    };

    const rejectingFn = async () => assert.fail();

    await assert.rejects(assert.doesNotReject(rejectingFn, common.mustCall(handler1)), common.mustCall(handler2));

    await assert.rejects(assert.doesNotReject(rejectingFn(), common.mustCall(handler1)), common.mustCall(handler2));

    await assert.rejects(assert.doesNotReject(() => assert.fail(), common.mustNotCall()), common.mustCall(handler1));

    await assert.rejects(
      assert.doesNotReject(123),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        message: 'The "promiseFn" argument must be of type ' +
                'function or an instance of Promise. Received type number (123)'
      });
  });
});
/* eslint-enable no-restricted-syntax */
