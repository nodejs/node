'use strict';

const common = require('../../common');
const assert = require('assert');
const path = require('path');
const spawnSync = require('child_process').spawnSync;
const helper = require('../../common/report.js');
const tmpdir = require('../../common/tmpdir');

const binding = path.resolve(__dirname, `./build/${common.buildType}/binding`);

if (process.argv[2] === 'child') {
  (function childMain() {
    const addon = require(binding);
    addon.triggerFatalError();
  })();
  return;
}

const ARGS = [
  __filename,
  'child',
];

{
  // Verify that --report-on-fatalerror is respected when set.
  tmpdir.refresh();
  const args = ['--report-on-fatalerror', ...ARGS];
  const child = spawnSync(process.execPath, args, { cwd: tmpdir.path });
  assert.notStrictEqual(child.status, 0, 'Process exited unexpectedly');

  const reports = helper.findReports(child.pid, tmpdir.path);
  assert.strictEqual(reports.length, 1);

  const report = reports[0];
  helper.validate(report);

  const content = require(report);
  assert.strictEqual(content.header.trigger, 'FatalError');

  // Check that the javascript stack is present.
  assert.strictEqual(content.javascriptStack.stack.findIndex((frame) => frame.match('childMain')), 0);
}

{
  // Verify that --report-on-fatalerror is respected when not set.
  const args = ARGS;
  const child = spawnSync(process.execPath, args, { cwd: tmpdir.path });
  assert.notStrictEqual(child.status, 0, 'Process exited unexpectedly');
  const reports = helper.findReports(child.pid, tmpdir.path);
  assert.strictEqual(reports.length, 0);
}
