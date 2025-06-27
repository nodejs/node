'use strict';

// Testcases for situations involving fatal errors, like Javascript heap OOM

require('../common');
const assert = require('assert');
const helper = require('../common/report.js');
const spawnSync = require('child_process').spawnSync;
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');

// Common args that will cause an out-of-memory error for child process.
const ARGS = [
  '--max-heap-size=20',
  fixtures.path('report-oom'),
];
const REPORT_FIELDS = [
  ['header.trigger', 'OOMError'],
  ['javascriptHeap.memoryLimit', 20 * 1024 * 1024 /* 20MB */],
];

{
  // Verify that --report-on-fatalerror is respected when set.
  tmpdir.refresh();
  const args = ['--report-on-fatalerror', ...ARGS];
  const child = spawnSync(process.execPath, args, { cwd: tmpdir.path });
  assert.notStrictEqual(child.status, 0, 'Process exited unexpectedly');

  const reports = helper.findReports(child.pid, tmpdir.path);
  assert.strictEqual(reports.length, 1);

  const report = reports[0];
  helper.validate(report, REPORT_FIELDS);
}
