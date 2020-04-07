'use strict';

// Testcases for situations involving fatal errors, like Javascript heap OOM

require('../common');
const assert = require('assert');
const fs = require('fs');
const helper = require('../common/report.js');
const spawnSync = require('child_process').spawnSync;
const tmpdir = require('../common/tmpdir');

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
}

// Common args that will cause an out-of-memory error for child process.
const ARGS = [
  '--max-old-space-size=20',
  __filename,
  'child'
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

  // Errors occur in a context where env is not available, so thread ID is
  // unknown. Assert this, to verify that the underlying env-less situation is
  // actually reached.
  assert.strictEqual(require(report).header.threadId, null);
}

{
  // Verify that --report-on-fatalerror is respected when not set.
  const args = ARGS;
  const child = spawnSync(process.execPath, args, { cwd: tmpdir.path });
  assert.notStrictEqual(child.status, 0, 'Process exited unexpectedly');
  const reports = helper.findReports(child.pid, tmpdir.path);
  assert.strictEqual(reports.length, 0);
}

{
  // Verify that --report-directory is respected when set.
  // Verify that --report-compact is respected when not set.
  tmpdir.refresh();
  const dir = '--report-directory=' + tmpdir.path;
  const args = ['--report-on-fatalerror', dir, ...ARGS];
  const child = spawnSync(process.execPath, args, { });
  assert.notStrictEqual(child.status, 0, 'Process exited unexpectedly');

  const reports = helper.findReports(child.pid, tmpdir.path);
  assert.strictEqual(reports.length, 1);

  const report = reports[0];
  helper.validate(report);
  assert.strictEqual(require(report).header.threadId, null);
  const lines = fs.readFileSync(report, 'utf8').split('\n').length - 1;
  assert(lines > 10);
}

{
  // Verify that --report-compact is respected when set.
  tmpdir.refresh();
  const args = ['--report-on-fatalerror', '--report-compact', ...ARGS];
  const child = spawnSync(process.execPath, args, { cwd: tmpdir.path });
  assert.notStrictEqual(child.status, 0, 'Process exited unexpectedly');

  const reports = helper.findReports(child.pid, tmpdir.path);
  assert.strictEqual(reports.length, 1);

  const report = reports[0];
  helper.validate(report);
  assert.strictEqual(require(report).header.threadId, null);
  // Subtract 1 because "xx\n".split("\n") => [ 'xx', '' ].
  const lines = fs.readFileSync(report, 'utf8').split('\n').length - 1;
  assert.strictEqual(lines, 1);
}

{
  // Verify that --report-compact is respected when set.
  // Verify that --report-filename is respected when set.
  tmpdir.refresh();
  const args = [
    '--report-on-fatalerror',
    '--report-compact',
    '--report-filename=stderr',
    ...ARGS
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
