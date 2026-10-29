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

{
  tmpdir.refresh();
  // Verify that --report-on-fatalerror is respected when not set.
  const args = ARGS;
  const child = spawnSync(process.execPath, args, { cwd: tmpdir.path });
  assert.notStrictEqual(child.status, 0);
  const reports = helper.findReports(child.pid, tmpdir.path);
  assert.strictEqual(reports.length, 0);
}
