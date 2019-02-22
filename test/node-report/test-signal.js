'use strict';
// Testcase to produce report on signal interrupting a js busy-loop,
// showing it is interruptible.
const common = require('../common');
common.skipIfReportDisabled();

if (common.isWindows) return common.skip('Unsupported on Windows.');

if (process.argv[2] === 'child') {
  // Exit on loss of parent process
  const exit = () => process.exit(2);
  process.on('disconnect', exit);

  function busyLoop() {
    setInterval(() => {
      const list = [];
      for (let i = 0; i < 1e3; i++) {
        for (let j = 0; j < 1000; j++) {
          list.push(new MyRecord());
        }
        for (let k = 0; k < 1000; k++) {
          list[k].id += 1;
          list[k].account += 2;
        }
        for (let l = 0; l < 1000; l++) {
          list.pop();
        }
      }
    }, 1000);
  }

  function MyRecord() {
    this.name = 'foo';
    this.id = 128;
    this.account = 98454324;
  }
  process.send('child started', busyLoop);


} else {
  const helper = require('../common/report.js');
  const fork = require('child_process').fork;
  const tmpdir = require('../common/tmpdir');
  tmpdir.refresh();
  const assert = require('assert');
  if (common.isWindows) {
    assert.fail('Unsupported on Windows', { skip: true });
    return;
  }
  console.log(tmpdir.path);
  const options = { stdio: 'pipe', encoding: 'utf8', cwd: tmpdir.path };
  const child = fork('--experimental-report',
                     ['--diagnostic-report-on-signal', __filename, 'child'],
                     options);
  // Wait for child to indicate it is ready before sending signal
  child.on('message', () => child.kill('SIGUSR2'));
  let stderr = '';
  child.stderr.on('data', (chunk) => {
    stderr += chunk;
    // Terminate the child after the report has been written
    if (stderr.includes('Node.js report completed')) {
      child.kill('SIGTERM');
    }
  });
  child.on('exit', common.mustCall((code, signal) => {
    console.log('child exited');
    const report_msg = 'No reports found';
    const process_msg = 'Process exited unexpectedly';
    const signal_msg = 'Process exited with unexpected signal';
    assert.strictEqual(code, null, process_msg + ':' + code);
    assert.deepStrictEqual(signal, 'SIGTERM',
                           signal_msg + ':' + signal);
    const reports = helper.findReports(child.pid, tmpdir.path);
    assert.deepStrictEqual(reports.length, 1, report_msg);
    const report = reports[0];
    helper.validate(report);
  }));
}
