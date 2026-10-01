'use strict';

// Tests --report-on-process-timeout.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { spawnSync } = require('child_process');
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
