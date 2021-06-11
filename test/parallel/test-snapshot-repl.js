'use strict';

// This tests that user land snapshots works when the instance restored from
// the snapshot is launched as a REPL.

const common = require('../common');
if (common.isWindows) {
  // No way to send CTRL_C_EVENT to processes from JS right now.
  common.skip('platform not supported');
}
const assert = require('assert');
const { spawnSync, spawn } = require('child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const path = require('path');

tmpdir.refresh();
const blobPath = path.join(tmpdir.path, 'my-snapshot.blob');
const file = fixtures.path('snapshot', 'mutate-fs.js');

{
  // Create the snapshot.
  const child = spawnSync(process.execPath, [
    '--snapshot-main',
    file,
    '--snapshot-blob',
    blobPath,
  ], {
    cwd: tmpdir.path
  });
  if (child.status !== 0) {
    console.log(child.stderr.toString());
    console.log(child.stdout.toString());
    assert.strictEqual(child.status, 0);
  }
}

{
  // Use the snapshot to run the REPL.
  const child = spawn(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '-i',
  ], {
    cwd: tmpdir.path,
    env: {
      ...process.env,
      REPL_TEST_PPID: process.pid
    }
  });

  let stdout = '';
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (data) => {
    stdout += data;
  });

  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (data) => {
    stderr += data;
  });

  child.stdout.once('data', () => {
    process.on('SIGUSR2', () => {
      process.kill(child.pid, 'SIGINT');
      child.stdout.once('data', () => {
        // Make sure state from before the interruption is still available.
        child.stdin.end('fs.foo\n');
      });
    });

    child.stdin.write('process.kill(+process.env.REPL_TEST_PPID, "SIGUSR2");' +
                      'while(true){}\n');
  });

  child.on('close', common.mustCall(function(code) {
    if (code !== 0) {
      console.log(stderr);
      console.log(stdout);
      assert.strictEqual(code, 0);
    }
    // Check that fs.foo is mutated from the snapshot
    assert(/I am from the snapshot/.test(stdout));
  }));

  child.stdin.end("require('fs').foo");
}
