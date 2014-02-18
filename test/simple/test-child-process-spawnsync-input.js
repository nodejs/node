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

var common = require('../common');
var assert = require('assert');
var os = require('os');
var util = require('util');

var spawnSync = require('child_process').spawnSync;

var msgOut = 'this is stdout';
var msgErr = 'this is stderr';

// this is actually not os.EOL?
var msgOutBuf = new Buffer(msgOut + '\n');
var msgErrBuf = new Buffer(msgErr + '\n');

var args = [
  '-e',
  util.format('console.log("%s"); console.error("%s");', msgOut, msgErr)
];

var ret;


if (process.argv.indexOf('spawnchild') !== -1) {
  switch (process.argv[3]) {
    case '1':
      ret = spawnSync(process.execPath, args, { stdio: 'inherit' });
      common.checkSpawnSyncRet(ret);
      break;
    case '2':
      ret = spawnSync(process.execPath, args, {
        stdio: ['inherit', 'inherit', 'inherit']
      });
      common.checkSpawnSyncRet(ret);
      break;
  }
  process.exit(0);
  return;
}


function verifyBufOutput(ret) {
  common.checkSpawnSyncRet(ret);
  assert.deepEqual(ret.stdout, msgOutBuf);
  assert.deepEqual(ret.stderr, msgErrBuf);
}


verifyBufOutput(spawnSync(process.execPath, [__filename, 'spawnchild', 1]));
verifyBufOutput(spawnSync(process.execPath, [__filename, 'spawnchild', 2]));

var options = {
  input: 1234
};

assert.throws(function() {
  spawnSync('cat', [], options);
}, /TypeError:.*should be Buffer or string not number/);


options = {
  input: 'hello world'
};

ret = spawnSync('cat', [], options);

common.checkSpawnSyncRet(ret);
assert.strictEqual(ret.stdout.toString('utf8'), options.input);
assert.strictEqual(ret.stderr.toString('utf8'), '');

options = {
  input: new Buffer('hello world')
};

ret = spawnSync('cat', [], options);

common.checkSpawnSyncRet(ret);
assert.deepEqual(ret.stdout, options.input);
assert.deepEqual(ret.stderr, new Buffer(''));

verifyBufOutput(spawnSync(process.execPath, args));

ret = spawnSync(process.execPath, args, { encoding: 'utf8' });

common.checkSpawnSyncRet(ret);
assert.strictEqual(ret.stdout, msgOut + '\n');
assert.strictEqual(ret.stderr, msgErr + '\n');

options = {
  maxBuffer: 1
};

ret = spawnSync(process.execPath, args, options);

assert.ok(ret.error, 'maxBuffer should error');
assert.strictEqual(ret.error.errno, 'ENOBUFS');
// we can have buffers larger than maxBuffer because underneath we alloc 64k
// that matches our read sizes
assert.deepEqual(ret.stdout, msgOutBuf);
