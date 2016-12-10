'use strict';
require('../common');
const assert = require('assert');

const spawnSync = require('child_process').spawnSync;

const msgOut = 'this is stdout';

// This is actually not os.EOL?
const msgOutBuf = Buffer.from(msgOut + '\n');

const args = [
  '-e',
  `console.log("${msgOut}");`
];

const options = {
  maxBuffer: 1
};

const ret = spawnSync(process.execPath, args, options);

assert.ok(ret.error, 'maxBuffer should error');
assert.strictEqual(ret.error.errno, 'ENOBUFS');
// We can have buffers larger than maxBuffer because underneath we alloc 64k
// that matches our read sizes
assert.deepStrictEqual(ret.stdout, msgOutBuf);
