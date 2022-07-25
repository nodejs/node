'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { fork } = require('child_process');

// This test check the arguments of `fork` method
// Refs: https://github.com/nodejs/node/issues/20749
const expectedEnv = { foo: 'bar' };

// Ensure that first argument `modulePath` must be provided
// and be of type string
{
  const invalidModulePath = [
    0,
    true,
    undefined,
    null,
    [],
    {},
    () => {},
    Symbol('t'),
  ];
  invalidModulePath.forEach((modulePath) => {
    assert.throws(() => fork(modulePath), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: /^The "modulePath" argument must be of type string/
    });
  });

  const cp = fork(fixtures.path('child-process-echo-options.js'));
  cp.on(
    'exit',
    common.mustCall((code) => {
      assert.strictEqual(code, 0);
    })
  );
}

// Ensure that the second argument of `fork`
// and `fork` should parse options
// correctly if args is undefined or null
{
  const invalidSecondArgs = [
    0,
    true,
    () => {},
    Symbol('t'),
  ];
  invalidSecondArgs.forEach((arg) => {
    assert.throws(
      () => {
        fork(fixtures.path('child-process-echo-options.js'), arg);
      },
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError'
      }
    );
  });

  const argsLists = [undefined, null, []];

  argsLists.forEach((args) => {
    const cp = fork(fixtures.path('child-process-echo-options.js'), args, {
      env: { ...process.env, ...expectedEnv }
    });

    cp.on(
      'message',
      common.mustCall(({ env }) => {
        assert.strictEqual(env.foo, expectedEnv.foo);
      })
    );

    cp.on(
      'exit',
      common.mustCall((code) => {
        assert.strictEqual(code, 0);
      })
    );
  });
}

// Ensure that the third argument should be type of object if provided
{
  const invalidThirdArgs = [
    0,
    true,
    () => {},
    Symbol('t'),
  ];
  invalidThirdArgs.forEach((arg) => {
    assert.throws(
      () => {
        fork(fixtures.path('child-process-echo-options.js'), [], arg);
      },
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError'
      }
    );
  });
}
