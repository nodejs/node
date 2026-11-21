// Flags: --expose-internals
'use strict';

// This verifies the error thrown by fs.watch.

const common = require('../common');

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const nonexistentFile = tmpdir.resolve('non-existent');
const { internalBinding } = require('internal/test/binding');
const {
  UV_ENODEV,
  UV_ENOENT
} = internalBinding('uv');
const { FSWatcher: RecursiveFSWatcher } = require('internal/fs/recursive_watch');
const { kFSWatchStart } = require('internal/fs/watchers');

tmpdir.refresh();

{
  assert.throws(
    () => fs.watch(nonexistentFile, common.mustNotCall()),
    (err) => {
      assert.strictEqual(err.path, nonexistentFile);
      assert.strictEqual(err.filename, nonexistentFile);
      assert.ok(err.syscall === 'watch' || err.syscall === 'stat');
      if (err.code === 'ENOENT') {
        assert.ok(err.message.startsWith('ENOENT: no such file or directory'));
        assert.strictEqual(err.errno, UV_ENOENT);
        assert.strictEqual(err.code, 'ENOENT');
      } else {  // AIX
        assert.strictEqual(
          err.message,
          `ENODEV: no such device, watch '${nonexistentFile}'`);
        assert.strictEqual(err.errno, UV_ENODEV);
        assert.strictEqual(err.code, 'ENODEV');
      }
      return true;
    },
  );
}

{
  assert.throws(
    () => fs.watch(nonexistentFile, { throwIfNoEntry: true }, common.mustNotCall()),
    {
      path: nonexistentFile,
      filename: nonexistentFile,
      code: /^(ENOENT|ENODEV)$/,
    },
  );
}

{
  if (common.isAIX) {
    assert.throws(
      () => fs.watch(nonexistentFile, { throwIfNoEntry: false }, common.mustNotCall()),
      { code: 'ENODEV' },
    );
  } else {
    const watcher = fs.watch(nonexistentFile, { throwIfNoEntry: false }, common.mustNotCall());
    if (common.isLinux) {
      setTimeout(common.mustCall(() => watcher.close()), common.platformTimeout(10));
    } else {
      watcher.close();
    }
  }
}

{
  assert.throws(
    () => fs.watch(nonexistentFile, {
      recursive: true,
      throwIfNoEntry: true,
    }, common.mustNotCall()),
    {
      path: nonexistentFile,
      filename: nonexistentFile,
      code: 'ENOENT',
    },
  );
}

{
  const watcher = fs.watch(nonexistentFile, {
    recursive: true,
    throwIfNoEntry: false,
  }, common.mustNotCall());
  watcher.close();
}

{
  const directory = tmpdir.resolve('recursive-watch-error');
  const expected = new Error('recursive watcher failed');
  const originalWatch = fs.watch;
  const watcher = new RecursiveFSWatcher({ recursive: true });
  const close = common.mustCall();
  let calls = 0;

  expected.code = 'ENOSPC';
  fs.mkdirSync(`${directory}/subdirectory`, { recursive: true });
  fs.watch = common.mustCall(() => {
    if (calls++ === 0) {
      return { close };
    }
    throw expected;
  }, 2);

  try {
    assert.throws(
      () => watcher[kFSWatchStart](directory),
      (error) => {
        assert.strictEqual(error, expected);
        assert.strictEqual(error.filename, directory);
        return true;
      },
    );
  } finally {
    fs.watch = originalWatch;
    watcher.close();
  }
}

{
  if (common.isMacOS || common.isWindows) {
    const file = tmpdir.resolve('file-to-watch');
    fs.writeFileSync(file, 'test');
    const watcher = fs.watch(file, common.mustNotCall());

    watcher.on('error', common.mustCall((err) => {
      assert.strictEqual(err.path, nonexistentFile);
      assert.strictEqual(err.filename, nonexistentFile);
      assert.strictEqual(
        err.message,
        `ENOENT: no such file or directory, watch '${nonexistentFile}'`);
      assert.strictEqual(err.errno, UV_ENOENT);
      assert.strictEqual(err.code, 'ENOENT');
      assert.strictEqual(err.syscall, 'watch');
      fs.unlinkSync(file);
      return true;
    }));

    // Simulate the invocation from the binding
    watcher._handle.onchange(UV_ENOENT, 'ENOENT', nonexistentFile);
  }
}
