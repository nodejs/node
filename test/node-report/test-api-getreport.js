// Flags: --experimental-report
'use strict';
const common = require('../common');
common.skipIfReportDisabled();
const assert = require('assert');
const helper = require('../common/report');

common.expectWarning('ExperimentalWarning',
                     'report is an experimental feature. This feature could ' +
                     'change at any time');
helper.validateContent(process.report.getReport());
assert.deepStrictEqual(helper.findReports(process.pid, process.cwd()), []);
