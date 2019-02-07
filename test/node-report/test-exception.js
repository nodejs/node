'use strict';

// Testcase to produce report on uncaught exception
const common = require('../common');
common.skipIfReportDisabled();
if (process.argv[2] === 'child') {
  function myException(request, response) {
    const m = '*** test-exception.js: throwing uncaught Error';
    throw new Error(m);
  }

  myException();

} else {
  const helper = require('../common/report.js');
  const tmpdir = require('../common/tmpdir');
  tmpdir.refresh();
  const spawn = require('child_process').spawn;
  const assert = require('assert');

  const child = spawn(process.execPath,
                      ['--experimental-report',
                       '--diagnostic-report-uncaught-exception',
                       __filename, 'child'],
                      { cwd: tmpdir.path });
  // Capture stderr output from the child process
  let stderr = '';
  child.stderr.on('data', (chunk) => {
    stderr += chunk;
  });
  child.on('exit', common.mustCall((code) => {
    const report_msg = 'No reports found';
    const process_msg = 'Process exited unexpectedly';
    assert.strictEqual(code, 1, process_msg + ':' + code);
    assert.ok(new RegExp('myException').test(stderr),
              'Check for expected stack trace frame in stderr');
    const reports = helper.findReports(child.pid, tmpdir.path);
    assert.strictEqual(reports.length, 1, report_msg);
    const report = reports[0];
    helper.validate(report);
  }));
}
