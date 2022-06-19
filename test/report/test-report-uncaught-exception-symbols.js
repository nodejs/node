// Flags: --report-uncaught-exception
'use strict';
// Test producing a report on uncaught exception.
const common = require('../common');
const assert = require('assert');
const helper = require('../common/report');
const tmpdir = require('../common/tmpdir');

const exception = Symbol('foobar');

tmpdir.refresh();
process.report.directory = tmpdir.path;

process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err, exception);
  const reports = helper.findReports(process.pid, tmpdir.path);
  assert.strictEqual(reports.length, 1);

  helper.validate(reports[0], [
    ['header.event', 'Exception'],
    ['javascriptStack.message', 'Symbol(foobar)'],
  ]);
}));

throw exception;
