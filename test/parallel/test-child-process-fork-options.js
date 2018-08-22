/**
 * fork should parse options correctly if args is undefined or null
 */

'use strict';
const common = require('../common');
const assert = require('assert');
const { fork } = require('child_process');
const fixtures = require('../common/fixtures');

const expectedEnv = { foo: 'bar' };
const cp = fork(fixtures.path('child-process-echo-options.js'), undefined,
                { env: Object.assign({}, expectedEnv) });

cp.on('message', common.mustCall(function({ env }) {
  assert.strictEqual(env.foo, expectedEnv.foo);
}));
