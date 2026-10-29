'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const {
  closeSync,
  openSync,
  fsync,
  fsyncSync,
  readFileSync,
  writeSync,
  write,
} = require('node:fs');
const { join } = require('node:path');
const { Utf8Stream } = require('node:fs');
const { isMainThread } = require('node:worker_threads');

if (process.argv[2] === 'child-stdout') {
  // stdout is redirected to a regular file by the parent.
  const stream = new Utf8Stream({
    fd: 1,
    minLength: 4096,
    fs: {
      fsync: common.mustCall((fd, cb) => {
        assert.strictEqual(fd, 1);
        fsync(fd, cb);
      }),
    },
  });
  stream.write('hello world\n');
  stream.flush(common.mustSucceed());
  return;
}

tmpdir.refresh();
if (isMainThread) {
  process.umask(0o000);
}

let fileCounter = 0;

function getTempFile() {
  return join(tmpdir.path, `fastutf8stream-${process.pid}-${Date.now()}-${fileCounter++}.log`);
}

runTests(false);
runTests(true);

// Errors from fsync meaning the fd cannot be synchronized (e.g. a pipe or
// TTY) or is already closed do not fail the flush.
for (const code of ['EBADF', 'EINVAL', 'ENOTSUP', 'EOPNOTSUPP', 'EROFS']) {
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  const fsOverride = {
    fsync: common.mustCall((fd, cb) => {
      const err = new Error(code);
      err.code = code;
      process.nextTick(cb, err);
    }, 2),
  };
  const stream = new Utf8Stream({ fd, minLength: 4096, fs: fsOverride });

  stream.on('ready', common.mustCall(() => {
    assert.ok(stream.write('hello world\n'));
    stream.flush(common.mustSucceed(() => stream.end()));
  }));
}

// stdout redirected to a regular file is still fsynced by flush().
{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  const child = spawnSync(process.execPath, [__filename, 'child-stdout'], {
    stdio: ['ignore', fd, 'pipe'],
  });
  closeSync(fd);
  assert.strictEqual(child.status, 0, child.stderr.toString());
  assert.strictEqual(readFileSync(dest, 'utf8'), 'hello world\n');
}

function runTests(sync) {

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');

    const fsOverride = {
      fsync: common.mustNotCall(),
      fsyncSync: common.mustCall(() => fsyncSync(fd)),
    };
    if (sync) {
      fsOverride.writeSync = common.mustCall((...args) => writeSync(...args));
      fsOverride.write = common.mustNotCall();
    } else {
      fsOverride.write = common.mustCall((...args) => write(...args));
      fsOverride.writeSync = common.mustNotCall();
    }

    const stream = new Utf8Stream({
      fd,
      sync,
      fsync: true,
      minLength: 4096,
      fs: fsOverride,
    });

    stream.on('ready', common.mustCall(() => {
      assert.ok(stream.write('hello world\n'));

      stream.flush(common.mustSucceed(() => stream.end()));
    }));
  }

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');

    const testError = new Error('fsync failed');
    testError.code = 'ETEST';

    const fsOverride = {
      fsync: common.mustCall((fd, cb) => {
        process.nextTick(() => cb(testError));
      }, 2),
    };

    if (sync) {
      fsOverride.writeSync = common.mustCall((...args) => {
        return writeSync(...args);
      });
    } else {
      fsOverride.write = common.mustCall((...args) => {
        return write(...args);
      });
    }

    const stream = new Utf8Stream({
      fd,
      sync,
      minLength: 4096,
      fs: fsOverride,
    });

    stream.on('ready', common.mustCall(() => {
      assert.ok(stream.write('hello world\n'));
      stream.flush(common.mustCall((err) => {
        assert.ok(err, 'flush should return an error');
        assert.strictEqual(err.code, 'ETEST');
        stream.end();
      }));
    }));
  }
}
