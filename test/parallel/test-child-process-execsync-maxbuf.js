'use strict';
require('../common');

// This test checks that the maxBuffer option for child_process.spawnSync()
// works as expected.

const assert = require('assert');
const { execSync } = require('child_process');
const msgOut = 'this is stdout';
const msgOutBuf = Buffer.from(`${msgOut}\n`);

const args = [
  '-e',
  `"console.log('${msgOut}')";`
];

// Verify that an error is returned if maxBuffer is surpassed.
{
  assert.throws(() => {
    execSync(`"${process.execPath}" ${args.join(' ')}`, { maxBuffer: 1 });
  }, (e) => {
    assert.ok(e, 'maxBuffer should error');
    assert.strictEqual(e.errno, 'ENOBUFS');
    assert.deepStrictEqual(e.stdout, msgOutBuf);
    return true;
  });
}

// Verify that a maxBuffer size of Infinity works.
{
  const ret = execSync(
    `"${process.execPath}" ${args.join(' ')}`,
    { maxBuffer: Infinity }
  );

  assert.deepStrictEqual(ret, msgOutBuf);
}
