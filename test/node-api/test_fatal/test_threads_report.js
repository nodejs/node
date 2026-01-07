'use strict';
// Addons: test_fatal, test_fatal_vtable

const common = require('../../common');
const { addonPath, isInvokedAsChild, spawnTestSync } = require('../../common/addon-test');
const helper = require('../../common/report.js');
const tmpdir = require('../../common/tmpdir');

const assert = require('assert');
const test_fatal = require(addonPath);

if (common.buildType === 'Debug')
  common.skip('as this will currently fail with a Debug check ' +
              'in v8::Isolate::GetCurrent()');

// Test in a child process because the test code will trigger a fatal error
// that crashes the process.
if (isInvokedAsChild) {
  test_fatal.TestThread();
  while (true) {
    // Busy loop to allow the work thread to abort.
  }
} else {
  tmpdir.refresh();
  const p = spawnTestSync(['--report-on-fatalerror'], { cwd: tmpdir.path });
  assert.ifError(p.error);
  assert.ok(p.stderr.toString().includes(
    'FATAL ERROR: work_thread foobar'));
  assert.ok(p.status === 134 || p.signal === 'SIGABRT');

  const reports = helper.findReports(p.pid, tmpdir.path);
  assert.strictEqual(reports.length, 1);

  const report = reports[0];
  helper.validate(report);
}
