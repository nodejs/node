'use strict';

// Tests --diagnostic-report-verbose option.
const common = require('../common');
common.skipIfReportDisabled();
if (process.argv[2] === 'child') {
  // no-op
} else {
  const helper = require('../common/report.js');
  const spawn = require('child_process').spawn;
  const assert = require('assert');
  const tmpdir = require('../common/tmpdir');
  tmpdir.refresh();

  const expected = [ 'report: initialization complete, event flags:',
                     'report_uncaught_exception: 0',
                     'report_on_signal: 0',
                     'report_on_fatalerror: 0',
                     'report_signal:',
                     'report_filename:',
                     'report_directory:',
                     'report_verbose: 1' ];

  const child = spawn(process.execPath,
                      ['--experimental-report',
                       '--diagnostic-report-verbose',
                       __filename,
                       'child',
                      ],
                      { cwd: tmpdir.path });
  let stderr;
  child.stderr.on('data', (data) => stderr += data);
  child.on('exit', common.mustCall((code) => {
    const process_msg = 'Process exited unexpectedly';
    assert.strictEqual(code, 0, process_msg + ':' + code);
    const reports = helper.findReports(child.pid, tmpdir.path);
    assert.strictEqual(reports.length, 0,
                       `Found unexpected report ${reports[0]}`);
    for (const line of expected) {
      assert.ok(stderr.includes(line), `'${line}' not found in '${stderr}'`);
    }
  }));
}
