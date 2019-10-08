'use strict';

const common = require('../common');
common.skipIfReportDisabled();
const assert = require('assert');
// Testcase to produce report on fatal error (javascript heap OOM)
if (process.argv[2] === 'child') {

  const list = [];
  while (true) {
    const record = new MyRecord();
    list.push(record);
  }

  function MyRecord() {
    this.name = 'foo';
    this.id = 128;
    this.account = 98454324;
  }
} else {
  const helper = require('../common/report.js');
  const tmpdir = require('../common/tmpdir');
  tmpdir.refresh();
  const spawnSync = require('child_process').spawnSync;
  let args = ['--experimental-report',
              '--report-on-fatalerror',
              '--max-old-space-size=20',
              __filename,
              'child'];

  let child = spawnSync(process.execPath, args, { cwd: tmpdir.path });

  assert.notStrictEqual(child.status, 0, 'Process exited unexpectedly');
  let reports = helper.findReports(child.pid, tmpdir.path);
  assert.strictEqual(reports.length, 1);
  const report = reports[0];
  helper.validate(report);

  args = ['--max-old-space-size=20',
          __filename,
          'child'];

  child = spawnSync(process.execPath, args, { cwd: tmpdir.path });
  assert.notStrictEqual(child.status, 0, 'Process exited unexpectedly');
  reports = helper.findReports(child.pid, tmpdir.path);
  assert.strictEqual(reports.length, 0);
}
