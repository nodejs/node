'use strict';

const common = require('../common');

const assert = require('assert');
const { spawn } = require('child_process');

const command = common.isWindows ? 'echo $Env:POLLUTED' : 'echo $POLLUTED';
const buffers = [];

Object.prototype.POLLUTED = 'yes!';

assert.deepStrictEqual(({}).POLLUTED, 'yes!');

const cp = spawn(command, { shell: true });

cp.stdout.on('data', common.mustCall(buffers.push.bind(buffers)));

cp.stdout.on('end', () => {
  const result = Buffer.concat(buffers).toString().trim();
  assert.strictEqual(result, '');
  delete Object.prototype.POLLUTED;
});
