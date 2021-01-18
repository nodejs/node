'use strict';

const common = require('../common');
const assert = require('assert');
const { promisify } = require('util');
const execFile = require('child_process').execFile;
const fixtures = require('../common/fixtures');

const echoFixture = fixtures.path('echo.js');
const promisified = promisify(execFile);
const invalidArgTypeError = {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError'
};

{
  // Verify that the signal option works properly
  const ac = new AbortController();
  const signal = ac.signal;
  const promise = promisified(process.execPath, [echoFixture, 0], { signal });

  ac.abort();

  promise.catch(common.mustCall((e) => {
    assert.strictEqual(e.name, 'AbortError');
  }));
}

{
  // Verify that the signal option works properly when already aborted
  const ac = new AbortController();
  const { signal } = ac;
  ac.abort();

  const promise = promisified(process.execPath, [echoFixture, 0], { signal });

  promise.catch(common.mustCall((e) => {
    assert.strictEqual(e.name, 'AbortError');
  }));
}

{
  // Verify that if something different than Abortcontroller.signal
  // is passed, ERR_INVALID_ARG_TYPE is thrown
  const signal = {};
  assert.throws(() => {
    promisified(process.execPath, [echoFixture, 0], { signal });
  }, invalidArgTypeError);
}

{
  // Verify that if something different than Abortcontroller.signal
  // is passed, ERR_INVALID_ARG_TYPE is thrown
  const signal = 'world!';
  assert.throws(() => {
    promisified(process.execPath, [echoFixture, 0], { signal });
  }, invalidArgTypeError);
}
