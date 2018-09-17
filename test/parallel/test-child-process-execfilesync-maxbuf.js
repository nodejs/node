'use strict';
require('../common');

// This test checks that the maxBuffer option for child_process.spawnSync()
// works as expected.

const assert = require('assert');
const execFileSync = require('child_process').execFileSync;
const msgOut = 'this is stdout';
const msgOutBuf = Buffer.from(`${msgOut}\n`);

const args = [
  '-e',
  `console.log("${msgOut}");`
];

// Verify that an error is returned if maxBuffer is surpassed.
{
  assert.throws(
    () => execFileSync(process.execPath, args, { maxBuffer: 1 }),
    (e) => {
      assert.ok(e, 'maxBuffer should error');
      assert.strictEqual(e.errno, 'ENOBUFS');
      assert.deepStrictEqual(e.stdout, msgOutBuf);
      return true;
    }
  );
}

// Verify that a maxBuffer size of Infinity works.
{
  const ret = execFileSync(process.execPath, args, { maxBuffer: Infinity });

  assert.deepStrictEqual(ret, msgOutBuf);
}

// maxBuffer size is Infinity at default.
{
  const ret = execFileSync(
    process.execPath,
    ['-e', "console.log('a'.repeat(200 * 1024))"],
    { encoding: 'utf-8' }
  );

  assert.ifError(ret.error);
}
