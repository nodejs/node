const common = require('../common');
const assert = require('assert');
const reportCommon = require('../common/node-report');

if (common.isWindows)
  common.skip('signals not supported on Windows');
process.kill(process.pid, 'SIGUSR2');
const report = reportCommon.findReports(process.pid);
assertStrictEquals(report.length, 1);

