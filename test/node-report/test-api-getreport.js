'use strict';

// Testcase for returning report as a string via API call
const common = require('../common');
common.skipIfReportDisabled();
const assert = require('assert');
if (process.argv[2] === 'child') {
  console.log(process.report.getReport());
} else {
  const helper = require('../common/report.js');
  const spawnSync = require('child_process').spawnSync;
  const tmpdir = require('../common/tmpdir');
  tmpdir.refresh();

  const args = ['--experimental-report', __filename, 'child'];
  const child = spawnSync(process.execPath, args, { cwd: tmpdir.path });
  const report_msg = 'Found report files';
  const std_msg = 'Found messages on stderr';
  assert.ok(child.stderr.toString().includes(
    `(node:${child.pid}) ExperimentalWarning: report is an` +
    ' experimental feature. This feature could change at any time'), std_msg);
  const reportFiles = helper.findReports(child.pid, tmpdir.path);
  assert.deepStrictEqual(reportFiles, [], report_msg);
  helper.validateContent(child.stdout);
}
