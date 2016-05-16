'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const cp = require('child_process');

let output = '';
const cmd = path.resolve(common.fixturesDir, 'child-process-shell-auto');
const proc = cp.spawn(cmd, {shell: 'auto'});
proc.stdout.on('data', (data) => {
  output += data;
});
proc.on('error', common.fail);
proc.on('close', common.mustCall((code) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(output.trim(), 'Success.');
}));
