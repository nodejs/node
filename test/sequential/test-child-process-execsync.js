'use strict';
var common = require('../common');
var assert = require('assert');

var execSync = require('child_process').execSync;
var execFileSync = require('child_process').execFileSync;

var TIMER = 200;
var SLEEP = 2000;

var start = Date.now();
var err;
var caught = false;

// Verify that stderr is not accessed when a bad shell is used
assert.throws(
  function() { execSync('exit -1', {shell: 'bad_shell'}); },
  /spawnSync bad_shell ENOENT/,
  'execSync did not throw the expected exception!'
);
assert.throws(
  function() { execFileSync('exit -1', {shell: 'bad_shell'}); },
  /spawnSync bad_shell ENOENT/,
  'execFileSync did not throw the expected exception!'
);

try {
  var cmd = `"${process.execPath}" -e "setTimeout(function(){}, ${SLEEP});"`;
  var ret = execSync(cmd, {timeout: TIMER});
} catch (e) {
  caught = true;
  assert.strictEqual(e.errno, 'ETIMEDOUT');
  err = e;
} finally {
  assert.strictEqual(ret, undefined, 'we should not have a return value');
  assert.strictEqual(caught, true, 'execSync should throw');
  var end = Date.now() - start;
  assert(end < SLEEP);
  assert(err.status > 128 || err.signal);
}

assert.throws(function() {
  execSync('iamabadcommand');
}, /Command failed: iamabadcommand/);

var msg = 'foobar';
var msgBuf = Buffer.from(msg + '\n');

// console.log ends every line with just '\n', even on Windows.

cmd = `"${process.execPath}" -e "console.log(\'${msg}\');"`;

ret = execSync(cmd);

assert.strictEqual(ret.length, msgBuf.length);
assert.deepStrictEqual(ret, msgBuf, 'execSync result buffer should match');

ret = execSync(cmd, { encoding: 'utf8' });

assert.strictEqual(ret, msg + '\n', 'execSync encoding result should match');

var args = [
  '-e',
  `console.log("${msg}");`
];
ret = execFileSync(process.execPath, args);

assert.deepStrictEqual(ret, msgBuf);

ret = execFileSync(process.execPath, args, { encoding: 'utf8' });

assert.strictEqual(ret, msg + '\n',
                   'execFileSync encoding result should match');

// Verify that the cwd option works - GH #7824
{
  const cwd = common.rootDir;
  const cmd = common.isWindows ? 'echo %cd%' : 'pwd';
  const response = execSync(cmd, {cwd});

  assert.strictEqual(response.toString().trim(), cwd);
}

// Verify that stderr is not accessed when stdio = 'ignore' - GH #7966
{
  assert.throws(function() {
    execSync('exit -1', {stdio: 'ignore'});
  }, /Command failed: exit -1/);
}
