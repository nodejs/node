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

const spawnSync = require('child_process').spawnSync;

const msgOut = 'this is stdout';
const msgErr = 'this is stderr';

// This is actually not os.EOL?
const msgOutBuf = Buffer.from(`${msgOut}\n`);
const msgErrBuf = Buffer.from(`${msgErr}\n`);

const args = [
  '-e',
  `console.log("${msgOut}"); console.error("${msgErr}");`,
];

let ret;


function checkSpawnSyncRet(ret) {
  assert.strictEqual(ret.status, 0);
  assert.strictEqual(ret.error, undefined);
}

function verifyBufOutput(ret) {
  checkSpawnSyncRet(ret);
  assert.deepStrictEqual(ret.stdout, msgOutBuf);
  assert.deepStrictEqual(ret.stderr, msgErrBuf);
}

if (process.argv.includes('spawnchild')) {
  switch (process.argv[3]) {
    case '1':
      ret = spawnSync(process.execPath, args, { stdio: 'inherit' });
      checkSpawnSyncRet(ret);
      break;
    case '2':
      ret = spawnSync(process.execPath, args, {
        stdio: ['inherit', 'inherit', 'inherit']
      });
      checkSpawnSyncRet(ret);
      break;
  }
  process.exit(0);
  return;
}

verifyBufOutput(spawnSync(process.execPath, [__filename, 'spawnchild', 1]));
verifyBufOutput(spawnSync(process.execPath, [__filename, 'spawnchild', 2]));

let options = {
  input: 1234
};

assert.throws(
  () => spawnSync('cat', [], options),
  { code: 'ERR_INVALID_ARG_TYPE', name: 'TypeError' });

options = {
  input: 'hello world'
};

ret = spawnSync('cat', [], options);

checkSpawnSyncRet(ret);
assert.strictEqual(ret.stdout.toString('utf8'), options.input);
assert.strictEqual(ret.stderr.toString('utf8'), '');

options = {
  input: Buffer.from('hello world')
};

ret = spawnSync('cat', [], options);

checkSpawnSyncRet(ret);
assert.deepStrictEqual(ret.stdout, options.input);
assert.deepStrictEqual(ret.stderr, Buffer.from(''));

// common.getArrayBufferViews expects a buffer
// with length an multiple of 8
const msgBuf = Buffer.from('hello world'.repeat(8));
for (const arrayBufferView of common.getArrayBufferViews(msgBuf)) {
  options = {
    input: arrayBufferView
  };

  ret = spawnSync('cat', [], options);

  checkSpawnSyncRet(ret);

  assert.deepStrictEqual(ret.stdout, msgBuf);
  assert.deepStrictEqual(ret.stderr, Buffer.from(''));
}

verifyBufOutput(spawnSync(process.execPath, args));

ret = spawnSync(process.execPath, args, { encoding: 'utf8' });

checkSpawnSyncRet(ret);
assert.strictEqual(ret.stdout, `${msgOut}\n`);
assert.strictEqual(ret.stderr, `${msgErr}\n`);
