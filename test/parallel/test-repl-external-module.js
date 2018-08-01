'use strict';

const common = require('../common');
const assert = require('assert');
const { fork } = require('child_process');

const child = fork('', {
  stdio: 'pipe',
  env: {
    ...process.env,
    NODE_EXTERNAL_REPL_MODULE: require.resolve('../fixtures/external_repl'),
  }
});

child.stdout.on('data', common.mustCall((d) => {
  assert.strictEqual(d, '42\n');
}));
