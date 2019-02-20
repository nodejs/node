// Flags: --experimental-report
'use strict';

// Test producing a report via API call, using the no-hooks/no-signal interface.
const common = require('../common');
common.skipIfReportDisabled();
const assert = require('assert');
const helper = require('../common/report');
const tmpdir = require('../common/tmpdir');

common.expectWarning('ExperimentalWarning',
                     'report is an experimental feature. This feature could ' +
                     'change at any time');
tmpdir.refresh();
process.report.setOptions({ path: tmpdir.path });
process.report.triggerReport();
const reports = helper.findReports(process.pid, tmpdir.path);
assert.strictEqual(reports.length, 1);
helper.validate(reports[0]);
