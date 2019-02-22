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
  const spawn = require('child_process').spawn;
  const args = ['--experimental-report',
                '--diagnostic-report-on-fatalerror',
                '--max-old-space-size=20',
                __filename,
                'child'];
  const child = spawn(process.execPath, args, { cwd: tmpdir.path });
  child.on('exit', common.mustCall((code) => {
    assert.notStrictEqual(code, 0, 'Process exited unexpectedly');
    const reports = helper.findReports(child.pid, tmpdir.path);
    assert.strictEqual(reports.length, 1);
    const report = reports[0];
    helper.validate(report);
  }));
}
