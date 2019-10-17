'use strict';

const common = require('../common');

const assert = require('assert');
const { spawnSync } = require('child_process');

const command = common.isWindows ? 'echo $Env:POLLUTED' : 'echo $POLLUTED';

Object.prototype.POLLUTED = 'yes!';

assert.deepStrictEqual(({}).POLLUTED, 'yes!');

const { stdout, stderr, error } = spawnSync(command, { shell: true });

assert.ifError(error);
assert.deepStrictEqual(stderr, Buffer.alloc(0));
assert.strictEqual(stdout.toString().trim(), '');

delete Object.prototype.POLLUTED;
