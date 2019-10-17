'use strict';

const common = require('../common');

const assert = require('assert');
const { execSync } = require('child_process');

const command = common.isWindows ? 'echo $Env:POLLUTED' : 'echo $POLLUTED';

Object.prototype.POLLUTED = 'yes!';

assert.deepStrictEqual(({}).POLLUTED, 'yes!');

const stdout = execSync(command, { shell: true });

assert.strictEqual(stdout.toString().trim(), '');

delete Object.prototype.POLLUTED;
