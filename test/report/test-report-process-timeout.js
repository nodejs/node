'use strict';

// Tests --report-on-process-timeout.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');
const helper = require('../common/report');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

// The deadline is measured from the start of the process, so on a slow machine
// it can expire before the script starts spinning. The script writes 'ready'
// to stdout before it does, and runs again with a longer timeout if it did not
// get that far.
let child;
for (let timeout = common.platformTimeout(1000); ; timeout *= 2) {
  child = spawnSync(process.execPath, [
    `--process-timeout=${timeout}ms`,
    '--report-on-process-timeout',
    '-e',
    `function spin() { for (;;); }
     require('fs').writeSync(1, 'ready\\n');
     spin();`,
  ], { cwd: tmpdir.path, encoding: 'utf8' });
  if (child.stdout === 'ready\n' || timeout >= common.platformTimeout(16000)) {
    break;
  }
}

assert.strictEqual(child.signal, null);
assert.strictEqual(child.status, 124, child.stderr);
assert.strictEqual(child.stdout, 'ready\n');
assert.match(child.stderr,
             /Main thread was executing JavaScript:[\s\S]*Writing Node\.js report to file: report\./);

const reports = helper.findReports(child.pid, tmpdir.path);
assert.strictEqual(reports.length, 1);
helper.validate(reports[0], [
  ['header.event', 'Process timed out (--process-timeout)'],
  ['header.trigger', 'ProcessTimeout'],
]);

const report = JSON.parse(fs.readFileSync(reports[0], 'utf8'));
assert.match(report.javascriptStack.stack[0], /^at spin \(\[eval\]:1:\d+\)$/);

{
  // A Worker thread that is blocked in a synchronous native call cannot
  // provide its part of the report. It is left out, so that the report is
  // completed before the process is forced to exit.
  // If the Worker thread was not blocked yet when the deadline expired, which
  // the fixture signals by creating a file, it is included in the report. Run
  // the fixture again with a longer timeout in that case.
  let child;
  let report;
  for (let timeout = common.platformTimeout(1000); ; timeout *= 2) {
    // Child processes of a previous attempt may still be running, so use a
    // different file for each attempt.
    const marker = tmpdir.resolve(`blocked-worker.${timeout}.ready`);
    child = spawnSync(process.execPath, [
      `--process-timeout=${timeout}ms`,
      '--report-on-process-timeout',
      fixtures.path('process-timeout', 'blocked-worker.js'),
      marker,
    ], { cwd: tmpdir.path, encoding: 'utf8' });

    const reports = helper.findReports(child.pid, tmpdir.path);
    assert.strictEqual(reports.length, 1, child.stderr);
    helper.validate(reports[0], [
      ['header.event', 'Process timed out (--process-timeout)'],
      ['header.trigger', 'ProcessTimeout'],
    ]);
    report = JSON.parse(fs.readFileSync(reports[0], 'utf8'));
    if ((fs.existsSync(marker) && report.workers.length === 0) ||
        timeout >= common.platformTimeout(16000)) {
      break;
    }
  }

  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.status, 124, child.stderr);
  assert.match(child.stderr, /^ {4}Worker \(thread 1, name 'blocked'\)$/m);
  // The report is completed before the process is forced to exit, so the
  // message about that is printed on its own line.
  assert.match(child.stderr, new RegExp(
    '^Writing Node\\.js report to file: report\\.\\S+\\.json\\n' +
    'Node\\.js report completed\\n' +
    '\\(node:\\d+\\) The process did not finish exiting within 5000ms after ' +
    '--process-timeout expired\\. Forcing exit\\.$', 'm'));
  assert.deepStrictEqual(report.workers, []);
}
