// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const assert = require('assert');

const { execFileSync, execSync, spawnSync } = require('child_process');
const { getSystemErrorName } = require('util');

const TIMER = 200;
const SLEEP = 2000;

const execOpts = { encoding: 'utf8', shell: true };

// Verify that stderr is not accessed when a bad shell is used
assert.throws(
  function() { execSync('exit -1', { shell: 'bad_shell' }); },
  /spawnSync bad_shell ENOENT/
);
assert.throws(
  function() { execFileSync('exit -1', { shell: 'bad_shell' }); },
  /spawnSync bad_shell ENOENT/
);

let caught = false;
let ret, err;
const start = Date.now();
try {
  const cmd = `"${process.execPath}" -e "setTimeout(function(){}, ${SLEEP});"`;
  ret = execSync(cmd, { timeout: TIMER });
} catch (e) {
  caught = true;
  assert.strictEqual(getSystemErrorName(e.errno), 'ETIMEDOUT');
  err = e;
} finally {
  assert.strictEqual(ret, undefined,
                     `should not have a return value, received ${ret}`);
  assert.ok(caught, 'execSync should throw');
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

const cmd = `"${process.execPath}" -e "console.log('${msg}');"`;

{
  const ret = execSync(cmd);
  assert.strictEqual(ret.length, msgBuf.length);
  assert.deepStrictEqual(ret, msgBuf);
}

{
  const ret = execSync(cmd, { encoding: 'utf8' });
  assert.strictEqual(ret, `${msg}\n`);
}

const args = [
  '-e',
  `console.log("${msg}");`
];
{
  const ret = execFileSync(process.execPath, args);
  assert.deepStrictEqual(ret, msgBuf);
}

{
  const ret = execFileSync(process.execPath, args, { encoding: 'utf8' });
  assert.strictEqual(ret, `${msg}\n`);
}

// Verify that the cwd option works.
// See https://github.com/nodejs/node-v0.x-archive/issues/7824.
{
  const cwd = common.rootDir;
  const cmd = common.isWindows ? 'echo %cd%' : 'pwd';
  const response = execSync(cmd, { cwd });

  assert.strictEqual(response.toString().trim(), cwd);
}

// Verify that stderr is not accessed when stdio = 'ignore'.
// See https://github.com/nodejs/node-v0.x-archive/issues/7966.
{
  assert.throws(function() {
    execSync('exit -1', { stdio: 'ignore' });
  }, /Command failed: exit -1/);
}

// Verify the execFileSync() behavior when the child exits with a non-zero code.
{
  const args = ['-e', 'process.exit(1)'];
  const spawnSyncResult = spawnSync(process.execPath, args);
  const spawnSyncKeys = Object.keys(spawnSyncResult).sort();
  assert.deepStrictEqual(spawnSyncKeys, [
    'output',
    'pid',
    'signal',
    'status',
    'stderr',
    'stdout'
  ]);

  assert.throws(() => {
    execFileSync(process.execPath, args);
  }, (err) => {
    const msg = `Command failed: ${process.execPath} ${args.join(' ')}`;

    assert(err instanceof Error);
    assert.strictEqual(err.message, msg);
    assert.strictEqual(err.status, 1);
    assert.strictEqual(typeof err.pid, 'number');
    spawnSyncKeys
      .filter((key) => key !== 'pid')
      .forEach((key) => {
        assert.deepStrictEqual(err[key], spawnSyncResult[key]);
      });
    return true;
  });
}

// Verify the shell option works properly
execFileSync(process.execPath, [], execOpts);
