const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { execFile } = require('child_process');
const reportCommon = require('../common/node-report');

if (common.isWindows)
  common.skip('signals not supported on Windows');

if (process.argv.includes('child')) {
  process.kill(process.pid, 'SIGUSR2');
  return;
}

tmpdir.refresh();
process.chdir(tmpdir.path);
let cp;
console.log("running");
//cp = execFile(process.execPath, ['--report-events', 'signal', __filename, 'child'], { cwd: tmpdir.path }, 
cp = execFile(process.execPath, ['--report-events', 'signal', __filename, 'child'], { cwd: tmpdir.path }, 
common.mustCall((err, stdout, stderr) => {
console.log(cp);
console.log(stdout);
console.log(stderr);
assert.ifError(err);
const report = reportCommon.findReports(cp.pid);
assert.strictEqual(report.length, 1);
reportCommon.validate(report[0], { pid: cp.pid });
}));


