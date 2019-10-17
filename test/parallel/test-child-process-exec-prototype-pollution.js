'use strict';

const common = require('../common');

const assert = require('assert');
const { exec } = require('child_process');

const command = common.isWindows ? 'echo $Env:POLLUTED' : 'echo $POLLUTED';

Object.prototype.POLLUTED = 'yes!';

assert.strictEqual(({}).POLLUTED, 'yes!');

exec(command, common.mustCall((err, stdout, stderr) => {
  assert.ifError(err);
  assert.strictEqual(stderr, '');
  assert.strictEqual(stdout.trim(), '');
  delete Object.prototype.POLLUTED;
}));
