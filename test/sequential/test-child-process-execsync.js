'use strict';
const common = require('../common');
const assert = require('assert');

const { execFileSync, execSync } = require('child_process');

const TIMER = 200;
const SLEEP = 2000;

const start = Date.now();
let err;
let caught = false;

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

let cmd, ret;
try {
  cmd = `"${process.execPath}" -e "setTimeout(function(){}, ${SLEEP});"`;
  ret = execSync(cmd, {timeout: TIMER});
} catch (e) {
  caught = true;
  assert.strictEqual(e.errno, 'ETIMEDOUT');
  err = e;
} finally {
  assert.strictEqual(ret, undefined,
                     `should not have a return value, received ${ret}`);
  assert.strictEqual(caught, true, 'execSync should throw');
  const end = Date.now() - start;
  assert(end < SLEEP);
  assert(err.status > 128 || err.signal);
}

assert.throws(function() {
  execSync('iamabadcommand');
}, /Command failed: iamabadcommand/);

const msg = 'foobar';
const msgBuf = Buffer.from(`${msg}\n`);

// console.log ends every line with just '\n', even on Windows.

cmd = `"${process.execPath}" -e "console.log('${msg}');"`;

ret = execSync(cmd);

assert.strictEqual(ret.length, msgBuf.length);
assert.deepStrictEqual(ret, msgBuf);

ret = execSync(cmd, { encoding: 'utf8' });

assert.strictEqual(ret, `${msg}\n`);

const args = [
  '-e',
  `console.log("${msg}");`
];
ret = execFileSync(process.execPath, args);

assert.deepStrictEqual(ret, msgBuf);

ret = execFileSync(process.execPath, args, { encoding: 'utf8' });

assert.strictEqual(ret, `${msg}\n`);

// Verify that the cwd option works.
// See https://github.com/nodejs/node-v0.x-archive/issues/7824.
{
  const cwd = common.rootDir;
  const cmd = common.isWindows ? 'echo %cd%' : 'pwd';
  const response = execSync(cmd, {cwd});

  assert.strictEqual(response.toString().trim(), cwd);
}

// Verify that stderr is not accessed when stdio = 'ignore'.
// See https://github.com/nodejs/node-v0.x-archive/issues/7966.
{
  assert.throws(function() {
    execSync('exit -1', {stdio: 'ignore'});
  }, /Command failed: exit -1/);
}

// Verify the execFileSync() behavior when the child exits with a non-zero code.
{
  const args = ['-e', 'process.exit(1)'];

  assert.throws(() => {
    execFileSync(process.execPath, args);
  }, (err) => {
    const msg = `Command failed: ${process.execPath} ${args.join(' ')}`;

    assert(err instanceof Error);
    assert.strictEqual(err.message.trim(), msg);
    assert.strictEqual(err.status, 1);
    return true;
  });
}
