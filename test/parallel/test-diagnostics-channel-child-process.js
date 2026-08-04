'use strict';
const common = require('../common');
const assert = require('assert');
const { spawn, ChildProcess } = require('child_process');
const dc = require('diagnostics_channel');
const path = require('path');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

const isChildProcess = (process) => process instanceof ChildProcess;

function testDiagnosticChannel(subscribers, test, after) {
  dc.tracingChannel('child_process.spawn').subscribe(subscribers);

  test(common.mustCall(() => {
    dc.tracingChannel('child_process.spawn').unsubscribe(subscribers);
    after?.();
  }));
}

const testSuccessfulSpawn = common.mustCall(() => {
  let cb;

  testDiagnosticChannel(
    {
      start: common.mustCall(({ process: childProcess, options }) => {
        assert.strictEqual(isChildProcess(childProcess), true);
        assert.strictEqual(options.file, process.execPath);
      }),
      end: common.mustCall(({ process: childProcess }) => {
        assert.strictEqual(isChildProcess(childProcess), true);
      }),
      error: common.mustNotCall(),
    },
    common.mustCall((callback) => {
      cb = callback;
      const child = spawn(process.execPath, ['-e', 'process.exit(0)']);
      child.on('close', () => {
        cb();
      });
    }),
    testFailingSpawnENOENT
  );
});

const testFailingSpawnENOENT = common.mustCall(() => {
  testDiagnosticChannel(
    {
      start: common.mustCall(({ process: childProcess, options }) => {
        assert.strictEqual(isChildProcess(childProcess), true);
        assert.strictEqual(options.file, 'does-not-exist');
      }),
      end: common.mustNotCall(),
      error: common.mustCall(({ process: childProcess, error }) => {
        assert.strictEqual(isChildProcess(childProcess), true);
        assert.strictEqual(error.code, 'ENOENT');
      }),
    },
    common.mustCall((callback) => {
      const child = spawn('does-not-exist');
      child.on('error', () => {});
      callback();
    }),
    common.isWindows ? undefined : testFailingSpawnEACCES,
  );
});

const testFailingSpawnEACCES = !common.isWindows ? common.mustCall(() => {
  tmpdir.refresh();
  const noExecFile = path.join(tmpdir.path, 'no-exec');
  fs.writeFileSync(noExecFile, '');
  fs.chmodSync(noExecFile, 0o644);

  testDiagnosticChannel(
    {
      start: common.mustCall(({ process: childProcess, options }) => {
        assert.strictEqual(isChildProcess(childProcess), true);
        assert.strictEqual(options.file, noExecFile);
      }),
      end: common.mustNotCall(),
      error: common.mustCall(({ process: childProcess, error }) => {
        assert.strictEqual(isChildProcess(childProcess), true);
        assert.strictEqual(error.code, 'EACCES');
      }),
    },
    common.mustCall((callback) => {
      const child = spawn(noExecFile);
      child.on('error', () => {});
      callback();
    }),
  );
}) : undefined;

testSuccessfulSpawn();
