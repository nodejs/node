'use strict';

// The timeout timer must clear on a spawn-time 'error', not just 'exit'.

const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

const start = Date.now();

const cp = spawn(process.execPath, ['--version'], {
  cwd: '/nonexistent/path/that/should/never/exist',
  timeout: common.platformTimeout(10000),
});

cp.on('error', common.mustCall((err) => {
  assert.strictEqual(err.code, 'ENOENT');
  assert.ok(Date.now() - start < 2000);
}));

cp.on('exit', common.mustNotCall());
