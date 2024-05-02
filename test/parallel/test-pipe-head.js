'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');

const exec = require('child_process').exec;

const nodePath = process.argv[0];
const script = fixtures.path('print-10-lines.js');

const cmd = `"${nodePath}" "${script}" | head -2`;

exec(cmd, common.mustSucceed((stdout, stderr) => {
  const lines = stdout.split('\n');
  assert.strictEqual(lines.length, 3);
}));
