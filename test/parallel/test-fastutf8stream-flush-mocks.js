'use strict';

require('../common');
const { test } = require('node:test');
const tmpdir = require('../common/tmpdir');
const {
  ok,
  strictEqual,
} = require('node:assert');
const {
  openSync,
  fsyncSync,
  writeSync,
  write,
  mkdirSync,
} = require('node:fs');
const { join } = require('node:path');
const { Utf8Stream } = require('node:fs');
const { isMainThread } = require('node:worker_threads');
const { once } = require('node:events');

tmpdir.refresh();
if (isMainThread) {
  process.umask(0o000);
}

let fileCounter = 0;

function getTempFile() {
  const testDir = join(tmpdir.path, `test-${process.pid}-${process.hrtime.bigint()}`);
  mkdirSync(testDir, { recursive: true });
  return join(testDir, `fastutf8stream-${fileCounter++}.log`);
}

function createClosePromise(stream) {
  return once(stream, 'close');
}


test('Utf8Stream sync flush with fsync success', async (t) => {
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  let fsyncSyncCalled = false;
  let writeSyncCalled = false;

  const fsOverride = {
    fsync: (fd, cb) => process.nextTick(cb),
    fsyncSync: () => {
      fsyncSyncCalled = true;
      return fsyncSync(fd);
    },
    writeSync: (...args) => {
      writeSyncCalled = true;
      return writeSync(...args);
    },
    write: t.assert.fail,
    close: (fd, cb) => process.nextTick(cb),
  };

  const stream = new Utf8Stream({
    fd,
    sync: true,
    fsync: true,
    minLength: 4096,
    fs: fsOverride,
  });

  const closePromise = createClosePromise(stream);

  await new Promise((resolve) => {
    stream.on('ready', () => {
      ok(stream.write('hello world\n'));

      stream.flush((err) => {
        t.assert.ifError(err);
        stream.end();
        resolve();
      });
    });
  });

  ok(fsyncSyncCalled, 'fsyncSync should have been called');
  ok(writeSyncCalled, 'writeSync should have been called');

  await closePromise;

});

test('Utf8Stream async flush with fsync success', async (t) => {
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  let fsyncSyncCalled = false;
  let writeCalled = false;

  const fsOverride = {
    fsync: (fd, cb) => process.nextTick(cb),
    fsyncSync: () => {
      fsyncSyncCalled = true;
      return fsyncSync(fd);
    },
    write: (...args) => {
      writeCalled = true;
      return write(...args);
    },
    writeSync: t.assert.fail,
    close: (fd, cb) => process.nextTick(cb),
  };

  const stream = new Utf8Stream({
    fd,
    sync: false,
    fsync: true,
    minLength: 4096,
    fs: fsOverride,
  });

  const closePromise = createClosePromise(stream);

  await new Promise((resolve) => {
    stream.on('ready', () => {
      ok(stream.write('hello world\n'));

      stream.flush((err) => {
        t.assert.ifError(err);
        stream.end();
        resolve();
      });
    });
  });

  ok(fsyncSyncCalled, 'fsyncSync should have been called');
  ok(writeCalled, 'write should have been called');

  await closePromise;

});

test('Utf8Stream sync flush with fsync error', async (t) => {
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  const testError = new Error('fsync failed');
  testError.code = 'ETEST';

  let fsyncCallCount = 0;
  let writeSyncCalled = false;

  const fsOverride = {
    fsync: (fd, cb) => {
      fsyncCallCount++;
      process.nextTick(() => cb(testError));
    },
    writeSync: (...args) => {
      writeSyncCalled = true;
      return writeSync(...args);
    },
    close: (fd, cb) => process.nextTick(cb),
  };

  const stream = new Utf8Stream({
    fd,
    sync: true,
    minLength: 4096,
    fs: fsOverride,
  });

  const closePromise = createClosePromise(stream);

  await new Promise((resolve) => {
    stream.on('ready', () => {
      ok(stream.write('hello world\n'));
      stream.flush((err) => {
        ok(err, 'flush should return an error');
        strictEqual(err.code, 'ETEST');
        stream.end();
        resolve();
      });
    });
  });

  strictEqual(fsyncCallCount, 2, 'fsync should have been called twice');
  ok(writeSyncCalled, 'writeSync should have been called');

  await closePromise;

});

test('Utf8Stream async flush with fsync error', async (t) => {
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  const testError = new Error('fsync failed');
  testError.code = 'ETEST';

  let fsyncCallCount = 0;
  let writeCalled = false;

  const fsOverride = {
    fsync: (fd, cb) => {
      fsyncCallCount++;
      process.nextTick(() => cb(testError));
    },
    write: (...args) => {
      writeCalled = true;
      return write(...args);
    },
    close: (fd, cb) => process.nextTick(cb),
  };

  const stream = new Utf8Stream({
    fd,
    sync: false,
    minLength: 4096,
    fs: fsOverride,
  });

  const closePromise = createClosePromise(stream);

  await new Promise((resolve) => {
    stream.on('ready', () => {
      ok(stream.write('hello world\n'));
      stream.flush((err) => {
        ok(err, 'flush should return an error');
        strictEqual(err.code, 'ETEST');
        stream.end();
        resolve();
      });
    });
  });

  strictEqual(fsyncCallCount, 2, 'fsync should have been called twice');
  ok(writeCalled, 'write should have been called');

  await closePromise;

});
