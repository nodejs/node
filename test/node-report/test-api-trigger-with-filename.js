'use strict';

// Tests when a report is triggered with a given filename.
const common = require('../common');
common.skipIfReportDisabled();
const filename = 'myreport.json';
if (process.argv[2] === 'child') {
  process.report.triggerReport(filename);
} else {
  const helper = require('../common/report.js');
  const spawn = require('child_process').spawn;
  const assert = require('assert');
  const { join } = require('path');
  const tmpdir = require('../common/tmpdir');
  tmpdir.refresh();

  const child = spawn(process.execPath,
                      ['--experimental-report', __filename, 'child'],
                      { cwd: tmpdir.path });
  child.on('exit', common.mustCall((code) => {
    const process_msg = 'Process exited unexpectedly';
    assert.strictEqual(code, 0, process_msg + ':' + code);
    const reports = helper.findReports(child.pid, tmpdir.path);
    assert.strictEqual(reports.length, 0,
                       `Found unexpected report ${reports[0]}`);
    const report = join(tmpdir.path, filename);
    helper.validate(report);
  }));
}
