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
  // Verify that --report-compact is respected when set.
  // Verify that --report-filename is respected when set.
  tmpdir.refresh();
  const args = [
    '--report-on-fatalerror',
    '--report-compact',
    '--report-filename=stderr',
    ...ARGS,
  ];
  const child = spawnSync(process.execPath, args, { encoding: 'utf8' });
  assert.notStrictEqual(child.status, 0, 'Process exited unexpectedly');

  const reports = helper.findReports(child.pid, tmpdir.path);
  assert.strictEqual(reports.length, 0);

  const lines = child.stderr.split('\n');
  // Skip over unavoidable free-form output and gc log from V8.
  const report = lines.find((i) => i.startsWith('{'));
  const json = JSON.parse(report);

  assert.strictEqual(json.header.threadId, null);
}
