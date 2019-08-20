'use strict';
require('../common');

// This test checks that the maxBuffer option for child_process.spawnSync()
// works as expected.

const assert = require('assert');
const spawnSync = require('child_process').spawnSync;
const { getSystemErrorName } = require('util');
const msgOut = 'this is stdout';
const msgOutBuf = Buffer.from(`${msgOut}\n`);

const args = [
  '-e',
  `console.log("${msgOut}");`
];

// Verify that an error is returned if maxBuffer is surpassed.
{
  const ret = spawnSync(process.execPath, args, { maxBuffer: 1 });

  assert.ok(ret.error, 'maxBuffer should error');
  assert.strictEqual(ret.error.code, 'ENOBUFS');
  assert.strictEqual(getSystemErrorName(ret.error.errno), 'ENOBUFS');
  // We can have buffers larger than maxBuffer because underneath we alloc 64k
  // that matches our read sizes.
  assert.deepStrictEqual(ret.stdout, msgOutBuf);
}

// Verify that a maxBuffer size of Infinity works.
{
  const ret = spawnSync(process.execPath, args, { maxBuffer: Infinity });

  assert.ifError(ret.error);
  assert.deepStrictEqual(ret.stdout, msgOutBuf);
}

// Default maxBuffer size is 1024 * 1024.
{
  const args = ['-e', "console.log('a'.repeat(1024 * 1024))"];
  const ret = spawnSync(process.execPath, args);

  assert.ok(ret.error, 'maxBuffer should error');
  assert.strictEqual(ret.error.code, 'ENOBUFS');
  assert.strictEqual(getSystemErrorName(ret.error.errno), 'ENOBUFS');
}

// Default maxBuffer size is 1024 * 1024.
{
  const args = ['-e', "console.log('a'.repeat(1024 * 1024 - 1))"];
  const ret = spawnSync(process.execPath, args);

  assert.ifError(ret.error);
  assert.deepStrictEqual(
    ret.stdout.toString().trim(),
    'a'.repeat(1024 * 1024 - 1)
  );
}
