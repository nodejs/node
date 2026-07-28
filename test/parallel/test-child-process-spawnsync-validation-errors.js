'use strict';
const common = require('../common');
const assert = require('assert');
const spawnSync = require('child_process').spawnSync;
const signals = require('os').constants.signals;
const rootUser = common.isWindows ? false :
  common.isIBMi ? true : process.getuid() === 0;

const invalidArgTypeError = { code: 'ERR_INVALID_ARG_TYPE', name: 'TypeError' };
const invalidRangeError = { code: 'ERR_OUT_OF_RANGE', name: 'RangeError' };

function pass(option, value) {
  // Run the command with the specified option. Since it's not a real command,
  // spawnSync() should run successfully but return an ENOENT error.
  const child = spawnSync('not_a_real_command', { [option]: value });

  assert.strictEqual(child.error.code, 'ENOENT');
}

function fail(option, value, message) {
  assert.throws(() => {
    spawnSync('not_a_real_command', { [option]: value });
  }, message);
}

{
  // Validate the cwd option
  pass('cwd', undefined);
  pass('cwd', null);
  pass('cwd', __dirname);
  fail('cwd', 0, invalidArgTypeError);
  fail('cwd', 1, invalidArgTypeError);
  fail('cwd', true, invalidArgTypeError);
  fail('cwd', false, invalidArgTypeError);
  fail('cwd', [], invalidArgTypeError);
  fail('cwd', {}, invalidArgTypeError);
  fail('cwd', common.mustNotCall(), invalidArgTypeError);
}

{
  // Validate the detached option
  pass('detached', undefined);
  pass('detached', null);
  pass('detached', true);
  pass('detached', false);
  fail('detached', 0, invalidArgTypeError);
  fail('detached', 1, invalidArgTypeError);
  fail('detached', __dirname, invalidArgTypeError);
  fail('detached', [], invalidArgTypeError);
  fail('detached', {}, invalidArgTypeError);
  fail('detached', common.mustNotCall(), invalidArgTypeError);
}

if (!common.isWindows) {
  {
    // Validate the uid option
    if (!rootUser) {
      pass('uid', undefined);
      pass('uid', null);
      pass('uid', process.getuid());
      fail('uid', __dirname, invalidArgTypeError);
      fail('uid', true, invalidArgTypeError);
      fail('uid', false, invalidArgTypeError);
      fail('uid', [], invalidArgTypeError);
      fail('uid', {}, invalidArgTypeError);
      fail('uid', common.mustNotCall(), invalidArgTypeError);
      fail('uid', NaN, invalidRangeError);
      fail('uid', Infinity, invalidRangeError);
      fail('uid', 3.1, invalidRangeError);
      fail('uid', -3.1, invalidRangeError);
    }
  }

  {
    // Validate the gid option
    if (process.getgid() !== 0) {
      pass('gid', undefined);
      pass('gid', null);
      pass('gid', process.getgid());
      fail('gid', __dirname, invalidArgTypeError);
      fail('gid', true, invalidArgTypeError);
      fail('gid', false, invalidArgTypeError);
      fail('gid', [], invalidArgTypeError);
      fail('gid', {}, invalidArgTypeError);
      fail('gid', common.mustNotCall(), invalidArgTypeError);
      fail('gid', NaN, invalidRangeError);
      fail('gid', Infinity, invalidRangeError);
      fail('gid', 3.1, invalidRangeError);
      fail('gid', -3.1, invalidRangeError);
    }
  }
}

{
  // Validate the shell option
  pass('shell', undefined);
  pass('shell', null);
  pass('shell', false);
  fail('shell', 0, invalidArgTypeError);
  fail('shell', 1, invalidArgTypeError);
  fail('shell', [], invalidArgTypeError);
  fail('shell', {}, invalidArgTypeError);
  fail('shell', common.mustNotCall(), invalidArgTypeError);
}

{
  // Validate the argv0 option
  pass('argv0', undefined);
  pass('argv0', null);
  pass('argv0', 'myArgv0');
  fail('argv0', 0, invalidArgTypeError);
  fail('argv0', 1, invalidArgTypeError);
  fail('argv0', true, invalidArgTypeError);
  fail('argv0', false, invalidArgTypeError);
  fail('argv0', [], invalidArgTypeError);
  fail('argv0', {}, invalidArgTypeError);
  fail('argv0', common.mustNotCall(), invalidArgTypeError);
}

{
  // Validate the windowsHide option
  pass('windowsHide', undefined);
  pass('windowsHide', null);
  pass('windowsHide', true);
  pass('windowsHide', false);
  fail('windowsHide', 0, invalidArgTypeError);
  fail('windowsHide', 1, invalidArgTypeError);
  fail('windowsHide', __dirname, invalidArgTypeError);
  fail('windowsHide', [], invalidArgTypeError);
  fail('windowsHide', {}, invalidArgTypeError);
  fail('windowsHide', common.mustNotCall(), invalidArgTypeError);
}

{
  // Validate the windowsVerbatimArguments option
  pass('windowsVerbatimArguments', undefined);
  pass('windowsVerbatimArguments', null);
  pass('windowsVerbatimArguments', true);
  pass('windowsVerbatimArguments', false);
  fail('windowsVerbatimArguments', 0, invalidArgTypeError);
  fail('windowsVerbatimArguments', 1, invalidArgTypeError);
  fail('windowsVerbatimArguments', __dirname, invalidArgTypeError);
  fail('windowsVerbatimArguments', [], invalidArgTypeError);
  fail('windowsVerbatimArguments', {}, invalidArgTypeError);
  fail('windowsVerbatimArguments', common.mustNotCall(), invalidArgTypeError);
}

{
  // Validate the timeout option
  pass('timeout', undefined);
  pass('timeout', null);
  pass('timeout', 1);
  pass('timeout', 0);
  fail('timeout', -1, invalidRangeError);
  fail('timeout', true, invalidArgTypeError);
  fail('timeout', false, invalidArgTypeError);
  fail('timeout', __dirname, invalidArgTypeError);
  fail('timeout', [], invalidArgTypeError);
  fail('timeout', {}, invalidArgTypeError);
  fail('timeout', common.mustNotCall(), invalidArgTypeError);
  fail('timeout', NaN, invalidRangeError);
  fail('timeout', Infinity, invalidRangeError);
  fail('timeout', 3.1, invalidRangeError);
  fail('timeout', -3.1, invalidRangeError);
}

{
  // Validate the maxBuffer option
  pass('maxBuffer', undefined);
  pass('maxBuffer', null);
  pass('maxBuffer', 1);
  pass('maxBuffer', 0);
  pass('maxBuffer', Infinity);
  pass('maxBuffer', 3.14);
  fail('maxBuffer', -1, invalidRangeError);
  fail('maxBuffer', NaN, invalidRangeError);
  fail('maxBuffer', -Infinity, invalidRangeError);
  fail('maxBuffer', true, invalidArgTypeError);
  fail('maxBuffer', false, invalidArgTypeError);
  fail('maxBuffer', __dirname, invalidArgTypeError);
  fail('maxBuffer', [], invalidArgTypeError);
  fail('maxBuffer', {}, invalidArgTypeError);
  fail('maxBuffer', common.mustNotCall(), invalidArgTypeError);
}

{
  // Validate the killSignal option
  const unknownSignalErr = { code: 'ERR_UNKNOWN_SIGNAL', name: 'TypeError' };

  pass('killSignal', undefined);
  pass('killSignal', null);
  pass('killSignal', 'SIGKILL');
  fail('killSignal', 'SIGNOTAVALIDSIGNALNAME', unknownSignalErr);
  fail('killSignal', true, invalidArgTypeError);
  fail('killSignal', false, invalidArgTypeError);
  fail('killSignal', [], invalidArgTypeError);
  fail('killSignal', {}, invalidArgTypeError);
  fail('killSignal', common.mustNotCall(), invalidArgTypeError);

  // Invalid signal names and numbers should fail
  fail('killSignal', 500, unknownSignalErr);
  fail('killSignal', 0, unknownSignalErr);
  fail('killSignal', -200, unknownSignalErr);
  fail('killSignal', 3.14, unknownSignalErr);

  Object.getOwnPropertyNames(Object.prototype).forEach((property) => {
    fail('killSignal', property, unknownSignalErr);
  });

  // Valid signal names and numbers should pass
  for (const signalName in signals) {
    pass('killSignal', signals[signalName]);
    pass('killSignal', signalName);
    pass('killSignal', signalName.toLowerCase());
  }
}
