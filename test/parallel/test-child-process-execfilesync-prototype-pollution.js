'use strict';

require('../common');

const fixtures = require('../common/fixtures');

const assert = require('assert');
const { execFileSync } = require('child_process');

const fixture = fixtures.path('child-process-echo-env.js');

Object.prototype.POLLUTED = 'yes!';

assert.strictEqual(({}).POLLUTED, 'yes!');

const stdout = execFileSync(process.execPath, [fixture, 'POLLUTED']);

assert.strictEqual(stdout.toString().trim(), 'undefined');

delete Object.prototype.POLLUTED;
