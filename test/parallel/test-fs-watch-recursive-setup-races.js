// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');
const { kFSWatchStart } = require('internal/fs/watchers');

// Exercise the AIX fallback behavior on every platform.
const platformDescriptor = Object.getOwnPropertyDescriptor(process, 'platform');
let FSWatcher;
try {
  Object.defineProperty(process, 'platform', {
    __proto__: null,
    configurable: true,
    enumerable: true,
    value: 'aix',
  });
  ({ FSWatcher } = require('internal/fs/recursive_watch'));
} finally {
  Object.defineProperty(process, 'platform', platformDescriptor);
}

function createWatcher() {
  return { close() {} };
}

tmpdir.refresh();

{
  const directory = tmpdir.resolve('watch-before-read');
  const originalReaddirSync = fs.readdirSync;
  const originalWatch = fs.watch;
  const operations = [];
  const watcher = new FSWatcher({ recursive: true });

  fs.mkdirSync(directory);
  fs.watch = common.mustCall((filename) => {
    assert.strictEqual(filename, directory);
    operations.push('watch');
    return createWatcher();
  });
  fs.readdirSync = common.mustCall((filename, ...args) => {
    assert.strictEqual(filename, directory);
    operations.push('read');
    return Reflect.apply(originalReaddirSync, fs, [filename, ...args]);
  });

  try {
    watcher[kFSWatchStart](directory);
    assert.deepStrictEqual(operations, ['watch', 'read']);
  } finally {
    watcher.close();
    fs.readdirSync = originalReaddirSync;
    fs.watch = originalWatch;
  }
}

{
  const directory = tmpdir.resolve('root-disappears-before-watch');
  const originalWatch = fs.watch;
  const expected = new Error('root disappeared');
  const watcher = new FSWatcher({ recursive: true });

  expected.code = 'ENODEV';
  fs.mkdirSync(directory);
  fs.watch = common.mustCall((filename) => {
    assert.strictEqual(filename, directory);
    fs.rmSync(directory, { recursive: true });
    throw expected;
  });

  try {
    assert.throws(
      () => watcher[kFSWatchStart](directory),
      (error) => {
        assert.strictEqual(error, expected);
        return true;
      },
    );
  } finally {
    watcher.close();
    fs.watch = originalWatch;
  }
}

{
  const directory = tmpdir.resolve('root-disappears-before-read');
  const originalWatch = fs.watch;
  const watcher = new FSWatcher({ recursive: true });
  let closeCalls = 0;

  fs.mkdirSync(directory);
  fs.watch = common.mustCall((filename) => {
    assert.strictEqual(filename, directory);
    fs.rmSync(directory, { recursive: true });
    return { close: () => closeCalls++ };
  });

  try {
    assert.throws(
      () => watcher[kFSWatchStart](directory),
      {
        code: 'ENOENT',
        filename: directory,
      },
    );
    assert.strictEqual(closeCalls, 1);
  } finally {
    watcher.close();
    fs.watch = originalWatch;
  }
}

{
  const directory = tmpdir.resolve('ignored-root-disappears-before-watch');
  const originalWatch = fs.watch;
  const watcher = new FSWatcher({ recursive: true, throwIfNoEntry: false });

  fs.mkdirSync(directory);
  fs.watch = common.mustCall((filename) => {
    assert.strictEqual(filename, directory);
    fs.rmSync(directory, { recursive: true });
    const error = new Error('root disappeared');
    error.code = 'ENODEV';
    throw error;
  });

  try {
    watcher[kFSWatchStart](directory);
  } finally {
    watcher.close();
    fs.watch = originalWatch;
  }
}

{
  const directory = tmpdir.resolve('retry-missing-directory-watch');
  const child = path.join(directory, 'child');
  const originalWatch = fs.watch;
  const watcher = new FSWatcher({ recursive: true });
  let childWatchCalls = 0;
  let directoryListener;
  let staleWatcherCloseCalls = 0;

  fs.mkdirSync(child, { recursive: true });
  fs.watch = common.mustCall((filename, options, listener) => {
    if (filename === directory) {
      directoryListener = listener;
      return createWatcher();
    }

    assert.strictEqual(filename, child);
    childWatchCalls++;
    if (childWatchCalls === 1) {
      fs.rmSync(child, { recursive: true });
      return { close: () => staleWatcherCloseCalls++ };
    }
    return createWatcher();
  }, 3);

  try {
    watcher[kFSWatchStart](directory);
    assert.strictEqual(childWatchCalls, 1);
    assert.strictEqual(staleWatcherCloseCalls, 1);

    fs.mkdirSync(child);
    directoryListener('rename', null);

    assert.strictEqual(childWatchCalls, 2);
  } finally {
    watcher.close();
    fs.watch = originalWatch;
  }
}

{
  const directory = tmpdir.resolve('retry-missing-watch');
  const file = path.join(directory, 'file');
  const originalWatch = fs.watch;
  const changes = [];
  const watcher = new FSWatcher({ recursive: true });
  let directoryListener;
  let fileWatchCalls = 0;

  fs.mkdirSync(directory);
  fs.writeFileSync(file, '');
  watcher.on('change', (eventType, filename) => changes.push([eventType, filename]));
  fs.watch = common.mustCall((filename, options, listener) => {
    if (filename === directory) {
      directoryListener = listener;
      return createWatcher();
    }

    assert.strictEqual(filename, file);
    fileWatchCalls++;
    if (fileWatchCalls === 1) {
      fs.rmSync(file);
      const error = new Error('path disappeared');
      error.code = 'ENODEV';
      throw error;
    }
    return createWatcher();
  }, 3);

  try {
    watcher[kFSWatchStart](directory);
    assert.strictEqual(fileWatchCalls, 1);

    fs.writeFileSync(file, '');
    directoryListener('rename', null);

    assert.strictEqual(fileWatchCalls, 2);
    assert.deepStrictEqual(changes, [['rename', 'file']]);
  } finally {
    watcher.close();
    fs.watch = originalWatch;
  }
}

{
  const directory = tmpdir.resolve('preserve-watch-error');
  const file = path.join(directory, 'file');
  const originalStatSync = fs.statSync;
  const originalWatch = fs.watch;
  const expected = new Error('watch failed');
  const verificationError = new Error('stat failed');
  const watcher = new FSWatcher({ recursive: true });

  expected.code = 'ENODEV';
  verificationError.code = 'EIO';
  fs.mkdirSync(directory);
  fs.writeFileSync(file, '');
  fs.watch = common.mustCall((filename) => {
    if (filename === directory) {
      return createWatcher();
    }
    assert.strictEqual(filename, file);
    throw expected;
  }, 2);
  fs.statSync = (filename, options) => {
    if (filename === file && options?.throwIfNoEntry === false) {
      throw verificationError;
    }
    return Reflect.apply(originalStatSync, fs, [filename, options]);
  };

  try {
    assert.throws(
      () => watcher[kFSWatchStart](directory),
      (error) => {
        assert.strictEqual(error, expected);
        return true;
      },
    );
  } finally {
    watcher.close();
    fs.statSync = originalStatSync;
    fs.watch = originalWatch;
  }
}
