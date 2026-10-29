// Flags: --report-uncaught-exception
'use strict';
// Test report is suppressed on uncaught exception hook.
const common = require('../common');
const assert = require('assert');
const helper = require('../common/report');
const tmpdir = require('../common/tmpdir');
const error = new Error('test error');

tmpdir.refresh();
process.report.directory = tmpdir.path;

// Make sure the uncaughtException listener is called.
process.on('uncaughtException', common.mustCall());

process.on('exit', (code) => {
  assert.strictEqual(code, 0);
  // Make sure no reports are generated.
  const reports = helper.findReports(process.pid, tmpdir.path);
  assert.strictEqual(reports.length, 0);
});

throw error;
