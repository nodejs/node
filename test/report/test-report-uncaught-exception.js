// Flags: --experimental-report --report-uncaught-exception
'use strict';
// Test producing a report on uncaught exception.
const common = require('../common');
common.skipIfReportDisabled();
const assert = require('assert');
const helper = require('../common/report');
const tmpdir = require('../common/tmpdir');
const error = new Error('test error');

common.expectWarning('ExperimentalWarning',
                     'report is an experimental feature. This feature could ' +
                     'change at any time');
tmpdir.refresh();
process.report.directory = tmpdir.path;

process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err, error);
  const reports = helper.findReports(process.pid, tmpdir.path);
  assert.strictEqual(reports.length, 1);
  helper.validate(reports[0]);
}));

throw error;
