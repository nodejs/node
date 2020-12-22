'use strict';

const common = require('../common');
const assert = require('assert');
const { promisify } = require('util');
const execFile = require('child_process').execFile;
const { getSystemErrorName } = require('util');
const fixtures = require('../common/fixtures');

const fixture = fixtures.path('exit.js');
const echoFixture = fixtures.path('echo.js');
const execOpts = { encoding: 'utf8', shell: true };
const execFilePromisified = promisify(execFile);

{
  // Verify that the signal option works properly
  const ac = new AbortController();
  const signal = ac.signal;
  const promise = execFilePromisified(process.execPath, [echoFixture, 0], { signal });
  
  ac.abort();

  assert.rejects(promise, /AbortError/);
}

{
  // Verify that the signal option works properly when already aborted
  const ac = new AbortController();
  const { signal } = ac;
  ac.abort();

  const promise = execFilePromisified(process.execPath, [echoFixture, 0], { signal });

  assert.rejects(promise, /AbortError/);
}

{
  // Verify that if something different than Abortcontroller.signal
  // is passed, ERR_INVALID_ARG_TYPE is thrown
  const promise = execFilePromisified(process.execPath, [echoFixture, 0], { signal: {} });

  assert.rejects(promise, {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "options.signal" property must be an ' +
             'instance of AbortSignal. Received an instance of Object'
  });

}

{
  // Verify that if something different than Abortcontroller.signal
  // is passed, ERR_INVALID_ARG_TYPE is thrown
  const promise = execFilePromisified(process.execPath, [echoFixture, 0], { signal: 'world!' });

  assert.rejects(promise, {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "options.signal" property must be an ' +
             `instance of AbortSignal. Received type string ('world!')`
  });

}
