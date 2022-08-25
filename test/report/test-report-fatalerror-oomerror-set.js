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
  '--max-old-space-size=20',
  fixtures.path('report-oom'),
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
  helper.validate(report);

  const content = require(report);
  // Errors occur in a context where env is not available, so thread ID is
  // unknown. Assert this, to verify that the underlying env-less situation is
  // actually reached.
  assert.strictEqual(content.header.threadId, null);
  assert.strictEqual(content.header.trigger, 'OOMError');
}
