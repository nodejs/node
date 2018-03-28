'use strict';
const common = require('../common');
const assert = require('assert');
const { promisify } = require('util');
const wait = promisify(setTimeout);

/* eslint-disable prefer-common-expectserror, no-restricted-properties */

// Test assert.rejects() and assert.doesNotReject() by checking their
// expected output and by verifying that they do not work sync

common.crashOnUnhandledRejection();

(async () => {
  await assert.rejects(
    () => assert.fail(),
    common.expectsError({
      code: 'ERR_ASSERTION',
      type: assert.AssertionError,
      message: 'Failed'
    })
  );

  await assert.doesNotReject(() => {});

  {
    const promise = assert.doesNotReject(async () => {
      await wait(1);
      throw new Error();
    });
    await assert.rejects(
      () => promise,
      (err) => {
        assert(err instanceof assert.AssertionError,
               `${err.name} is not instance of AssertionError`);
        assert.strictEqual(err.code, 'ERR_ASSERTION');
        assert(/^Got unwanted rejection\.\n$/.test(err.message));
        assert.strictEqual(err.operator, 'doesNotReject');
        assert.ok(!err.stack.includes('at Function.doesNotReject'));
        return true;
      }
    );
  }

  {
    const promise = assert.rejects(() => {});
    await assert.rejects(
      () => promise,
      (err) => {
        assert(err instanceof assert.AssertionError,
               `${err.name} is not instance of AssertionError`);
        assert.strictEqual(err.code, 'ERR_ASSERTION');
        assert(/^Missing expected rejection\.$/.test(err.message));
        assert.strictEqual(err.operator, 'rejects');
        assert.ok(!err.stack.includes('at Function.rejects'));
        return true;
      }
    );
  }
})().then(common.mustCall());
