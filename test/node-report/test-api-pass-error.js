'use strict';

// Testcase for passing an error object to the API call.
const common = require('../common');
common.skipIfReportDisabled();
const assert = require('assert');
if (process.argv[2] === 'child') {
  try {
    throw new Error('Testing error handling');
  } catch {
    process.report.triggerReport();
  }
} else {
  const helper = require('../common/report.js');
  const spawn = require('child_process').spawn;
  const tmpdir = require('../common/tmpdir');
  tmpdir.refresh();
  const child = spawn(process.execPath, ['--experimental-report',
                                         __filename, 'child'],
                      { cwd: tmpdir.path });
  child.on('exit', common.mustCall((code) => {
    const report_msg = 'No reports found';
    const process_msg = 'Process exited unexpectedly';
    assert.strictEqual(code, 0, process_msg);
    const reports = helper.findReports(child.pid, tmpdir.path);
    assert.strictEqual(reports.length, 1, report_msg);
    const report = reports[0];
    helper.validate(report);
  }));
}
