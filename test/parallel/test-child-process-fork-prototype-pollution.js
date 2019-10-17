'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');

const assert = require('assert');
const { fork } = require('child_process');

const fixture = fixtures.path('child-process-echo-options.js');

Object.prototype.POLLUTED = 'yes!';

assert.deepStrictEqual(({}).POLLUTED, 'yes!');

const cp = fork(fixture);

delete Object.prototype.POLLUTED;

cp.on('message', common.mustCall(({ env }) => {
  assert.strictEqual(env.POLLUTED, undefined);
}));

cp.on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 0);
}));
