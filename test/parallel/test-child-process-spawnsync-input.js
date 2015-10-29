'use strict';
var common = require('../common');
var assert = require('assert');
var os = require('os');

var spawnSync = require('child_process').spawnSync;

var msgOut = 'this is stdout';
var msgErr = 'this is stderr';

// this is actually not os.EOL?
var msgOutBuf = new Buffer(msgOut + '\n');
var msgErrBuf = new Buffer(msgErr + '\n');

var args = [
  '-e',
  `console.log("${msgOut}"); console.error("${msgErr}");`
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
