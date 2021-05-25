'use strict';
const common = require('../../common');
const assert = require('assert');
const child_process = require('child_process');
const test_fatal = require(`./build/${common.buildType}/test_fatal`);

if (common.buildType === 'Debug')
  common.skip('as this will currently fail with a Debug check ' +
              'in v8::Isolate::GetCurrent()');

// Test in a child process because the test code will trigger a fatal error
// that crashes the process.
if (process.argv[2] === 'child') {
  test_fatal.TestThread();
  // Busy loop to allow the work thread to abort.
  while (true) {}
}

const p = child_process.spawnSync(
  process.execPath, [ __filename, 'child' ]);
assert.ifError(p.error);
assert.ok(p.stderr.toString().includes(
  'FATAL ERROR: work_thread foobar'));
assert.ok(p.status === 134 || p.signal === 'SIGABRT');
