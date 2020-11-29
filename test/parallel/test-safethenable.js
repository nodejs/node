'use strict';

// Flags: --expose-internals --no-warnings

const common = require('../common');

const SafeThenable = require('internal/per_context/safethenable');
const assert = require('assert');

{
  new SafeThenable(Promise.resolve(1)).then(
    common.mustCall((t) => assert.strictEqual(t, 1))
  ).then(common.mustCall());
}
{
  new SafeThenable(Promise.reject(1)).catch(
    common.mustCall((t) => assert.strictEqual(t, 1))
  );
}
{
  new SafeThenable(Promise.reject(1)).then(
    common.mustNotCall(),
    common.mustCall((t) => assert.strictEqual(t, 1))
  );
}
{
  // Store original value to restore it later.
  const originalCatch = Promise.prototype.catch;

  Promise.prototype.catch = common.mustNotCall(function() {
    // User mutations of Promise prototype should not interfere.
    return this;
  });
  new SafeThenable(Promise.reject(1)).catch(
    common.mustCall((t) => assert.strictEqual(t, 1))
  ).then(common.mustCall());

  // Restore the original value.
  Promise.prototype.catch = originalCatch;
}
{
  assert.rejects(
    new SafeThenable(Promise.reject(new Error('Test error'))).then(
      common.mustNotCall()
    ),
    /Test error/
  ).then(common.mustCall());
}
{
  let wasAccessed = false;
  const thenable = {
    get then() {
      if (wasAccessed) throw new Error('Cannot access this method twice');
      wasAccessed = true;
      return function then(onSuccess, onError) {
        try {
          onSuccess();
        } catch (e) {
          onError(e);
        }
        return this;
      };
    },
  };

  assert(new SafeThenable(thenable).isThenable);

  assert.throws(
    () => typeof thenable.then === 'function',
    /Cannot access this method twice/
  );

  // SafeThenable allows to call then method even from new instances
  new SafeThenable(thenable).then(common.mustCall());
  new SafeThenable(thenable).catch(common.mustCall());
}
{
  [true, 1, 4n, 'string', Symbol(), function() {}, class {}, new class {}(),
   null, undefined, {}].forEach((val) => {
    assert(!new SafeThenable(val).isThenable);
    new SafeThenable(val).then(common.mustNotCall());
    assert.throws(() => new SafeThenable(val, true).then(common.mustNotCall()),
                  /not a function/);
  });
}
{
  Number.prototype.then = function then(onSuccess, onError) {
    try {
      onSuccess();
    } catch (e) {
      onError(e);
    }
  };
  assert(new SafeThenable(1).isThenable);
  new SafeThenable(1).then(common.mustCall());
  delete Number.prototype.then;
}
