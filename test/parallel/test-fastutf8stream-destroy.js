'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const {
  close,
  openSync,
  readFile,
  readFileSync,
  write,
} = require('node:fs');
const { Utf8Stream } = require('node:fs');
const { isMainThread } = require('node:worker_threads');
const { join } = require('node:path');

let fileCounter = 0;
tmpdir.refresh();

if (isMainThread) {
  process.umask(0o000);
}

function getTempFile() {
  return join(tmpdir.path, `fastutf8stream-${process.pid}-${Date.now()}-${fileCounter++}.log`);
}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  const stream = new Utf8Stream({ fd, sync: false });

  // Test successful write
  assert.ok(stream.write('hello world\n'));
  stream.destroy();

  assert.throws(() => stream.write('hello world\n'), Error);

  // Reading now would race the fs.write() still in flight.
  stream.once('write', common.mustCall(() => {
    readFile(dest, 'utf8', common.mustSucceed((data) => {
      assert.strictEqual(data, 'hello world\n');
    }));
  }));

  stream.on('finish', common.mustNotCall());
  stream.on('close', common.mustCall());
};

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  const stream = new Utf8Stream({ fd, sync: true });

  assert.ok(stream.write('hello world\n'));
  stream.destroy();
  assert.throws(() => stream.write('hello world\n'), Error);

  const data = readFileSync(dest, 'utf8');
  assert.strictEqual(data, 'hello world\n');
};

{
  const dest = getTempFile();
  const stream = new Utf8Stream({ dest });
  stream.destroy();
  stream.on('close', common.mustCall());
}

{
  // The fd is closed only after a write that is in flight at destroy() time
  // completes. Data buffered behind it is dropped, so 'finish' is not emitted.
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  let releaseWrite;
  const stream = new Utf8Stream({
    fd,
    fs: {
      write: common.mustCall((...args) => {
        releaseWrite = () => {
          releaseWrite = null;
          write(...args);
        };
      }),
      fsync: (fd, callback) => process.nextTick(callback),
      close: common.mustCall((fd, callback) => {
        assert.strictEqual(releaseWrite, null);
        close(fd, callback);
      }),
    },
  });

  stream.on('error', common.mustNotCall());
  stream.on('finish', common.mustNotCall());
  stream.on('close', common.mustCall(() => {
    assert.strictEqual(readFileSync(dest, 'utf8'), 'hello world\n');
  }));

  assert.ok(stream.write('hello world\n'));
  stream.write('dropped\n');
  stream.end();
  stream.destroy();
  setImmediate(() => releaseWrite());
}

{
  // A write started from an 'error' listener right before destroy() also
  // completes before the fd is closed.
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  let releaseWrite;
  const stream = new Utf8Stream({
    fd,
    fs: {
      write: common.mustCall((...args) => {
        if (releaseWrite === undefined) {
          releaseWrite = null;
          const err = new Error('EIO');
          err.code = 'EIO';
          process.nextTick(args.at(-1), err);
          return;
        }
        releaseWrite = () => {
          releaseWrite = null;
          write(...args);
        };
      }, 2),
      fsync: (fd, callback) => process.nextTick(callback),
      close: common.mustCall((fd, callback) => {
        assert.strictEqual(releaseWrite, null);
        close(fd, callback);
      }),
    },
  });

  stream.on('error', common.mustCall(() => {
    stream.write('second\n');
    stream.destroy();
    setImmediate(() => releaseWrite());
  }));
  stream.on('close', common.mustCall());

  assert.ok(stream.write('first\n'));
}

{
  // A pending write that fails after destroy() with no 'error' listener
  // throws, and the fd is still closed.
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  let failWrite;
  const stream = new Utf8Stream({
    fd,
    fs: {
      write: common.mustCall((fd, data, encoding, callback) => {
        failWrite = () => {
          const err = new Error('EIO');
          err.code = 'EIO';
          callback(err);
        };
      }),
    },
  });

  stream.on('close', common.mustCall());

  assert.ok(stream.write('hello world\n'));
  stream.destroy();
  setImmediate(common.mustCall(() => {
    process.once('uncaughtException', common.mustCall((err) => {
      if (err.code !== 'EIO') throw err;
    }));
    failWrite();
  }));
}

{
  // destroy() closes the fd even if fs.write() threw synchronously.
  const fd = openSync(getTempFile(), 'w');
  const stream = new Utf8Stream({
    fd,
    fs: {
      write: common.mustCall(() => {
        throw new Error('write failed');
      }),
    },
  });

  stream.on('close', common.mustCall());

  assert.throws(() => stream.write('hello world\n'), { message: 'write failed' });
  stream.destroy();
}
