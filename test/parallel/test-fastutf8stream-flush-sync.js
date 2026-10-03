'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const {
  openSync,
  readFile,
  readFileSync,
  writeSync,
} = require('node:fs');
const { Utf8Stream } = require('node:fs');
const { join } = require('node:path');
const { isMainThread } = require('node:worker_threads');


tmpdir.refresh();
let fileCounter = 0;

if (isMainThread) {
  process.umask(0o000);
}

function getTempFile() {
  return join(tmpdir.path, `fastutf8stream-${process.pid}-${Date.now()}-${fileCounter++}.log`);
}

runTests(false);
runTests(true);

function runTests(sync) {
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  const stream = new Utf8Stream({ fd, minLength: 4096, sync });

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  stream.flushSync();

  setImmediate(common.mustCall(() => {
    stream.end();
    const data = readFileSync(dest, 'utf8');
    assert.strictEqual(data, 'hello world\nsomething else\n');
    stream.on('close', common.mustCall());
  }));
}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  let reportEagain = true;

  const fsOverride = {
    writeSync: common.mustCall((...args) => {
      if (reportEagain) {
        reportEagain = false;
        const err = new Error('EAGAIN');
        err.code = 'EAGAIN';
        throw err;
      }
      writeSync(...args);
    }, 2),
  };

  const stream = new Utf8Stream({
    fd,
    sync: false,
    minLength: 1000,
    fs: fsOverride,
  });

  stream.on('ready', common.mustCall(() => {
    assert.ok(stream.write('hello world\n'));
    assert.ok(stream.write('something else\n'));
    stream.flushSync();
    stream.end();

    stream.on('finish', common.mustCall(() => {
      readFile(dest, 'utf8', common.mustSucceed((data) => {
        assert.strictEqual(data, 'hello world\nsomething else\n');
      }));
    }));
  }));
}

function toData(contentMode, text) {
  return contentMode === 'buffer' ? Buffer.from(text) : text;
}

// flushSync() while an asynchronous write is in flight writes the queued
// data without throwing, and the in-flight write still completes.
for (const contentMode of ['utf8', 'buffer']) {
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  let completeWrite;
  const fsOverride = {
    write: common.mustCall((fd, buf, ...args) => {
      const cb = args.pop();
      completeWrite = () => cb(null, writeSync(fd, buf));
    }),
  };
  const stream = new Utf8Stream({
    contentMode,
    fd,
    minLength: 0,
    sync: false,
    fs: fsOverride,
  });

  stream.on('ready', common.mustCall(() => {
    assert.ok(stream.write(toData(contentMode, 'in flight\n')));
    assert.strictEqual(stream.writing, true);
    assert.ok(stream.write(toData(contentMode, 'queued\n')));
    stream.flushSync();
    assert.strictEqual(readFileSync(dest, 'utf8'), 'queued\n');

    stream.flush(common.mustSucceed(() => {
      stream.on('finish', common.mustCall(() => {
        assert.strictEqual(readFileSync(dest, 'utf8'), 'queued\nin flight\n');
      }));
      stream.end();
    }));
    completeWrite();
  }));
}

// flushSync() while waiting to retry an EAGAIN write cancels the retry and
// writes everything synchronously, in order.
for (const contentMode of ['utf8', 'buffer']) {
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  const fsOverride = {
    write: common.mustCall((fd, buf, ...args) => {
      const cb = args.pop();
      const err = new Error('EAGAIN');
      err.code = 'EAGAIN';
      process.nextTick(cb, err);
    }),
  };
  const stream = new Utf8Stream({
    contentMode,
    fd,
    minLength: 0,
    sync: false,
    fs: fsOverride,
  });

  stream.on('ready', common.mustCall(() => {
    assert.ok(stream.write(toData(contentMode, 'hello\n')));
    // Wait for the EAGAIN to schedule the retry.
    setImmediate(common.mustCall(() => {
      assert.strictEqual(stream.writing, true);
      assert.ok(stream.write(toData(contentMode, 'world\n')));
      // A pending flush() completes once flushSync() takes over the write.
      stream.flush(common.mustSucceed(() => {
        stream.on('finish', common.mustCall(() => {
          assert.strictEqual(readFileSync(dest, 'utf8'), 'hello\nworld\n');
        }));
        stream.end();
      }));
      stream.flushSync();
      assert.strictEqual(stream.writing, false);
      assert.strictEqual(readFileSync(dest, 'utf8'), 'hello\nworld\n');
    }));
  }));
}

// flushSync() from a 'write' listener writes the unwritten remainder of a
// partial write before newly queued data.
for (const sync of [true, false]) {
  for (const contentMode of ['utf8', 'buffer']) {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');
    let partial = true;
    const partialWrite = (fd, buf) => {
      if (partial) {
        partial = false;
        return writeSync(fd, Buffer.from(buf).subarray(0, 5));
      }
      return writeSync(fd, buf);
    };
    const fsOverride = {
      writeSync: (fd, buf, ...args) => partialWrite(fd, buf),
      write: (fd, buf, ...args) => {
        const cb = args.pop();
        process.nextTick(cb, null, partialWrite(fd, buf));
      },
    };
    const stream = new Utf8Stream({
      contentMode,
      fd,
      minLength: 0,
      sync,
      fs: fsOverride,
    });

    stream.once('write', common.mustCall((n) => {
      assert.strictEqual(n, 5);
      assert.strictEqual(stream.writing, true);
      assert.ok(stream.write(toData(contentMode, 'next\n')));
      stream.flushSync();
      assert.strictEqual(readFileSync(dest, 'utf8'), 'hello world\nnext\n');
    }));

    stream.on('ready', common.mustCall(() => {
      assert.ok(stream.write(toData(contentMode, 'hello world\n')));
      stream.on('finish', common.mustCall(() => {
        assert.strictEqual(readFileSync(dest, 'utf8'), 'hello world\nnext\n');
      }));
      stream.end();
    }));
  }
}

// flushSync() while reopening writes the buffered data to the previous file,
// which stays open until the new file is ready.
{
  const dest = getTempFile();
  const stream = new Utf8Stream({ dest, minLength: 4096, sync: false });

  stream.once('ready', common.mustCall(() => {
    assert.ok(stream.write('before reopen\n'));
    stream.reopen();
    assert.strictEqual(stream.writing, true);
    stream.flushSync();
    assert.strictEqual(readFileSync(dest, 'utf8'), 'before reopen\n');
    stream.once('ready', common.mustCall(() => {
      stream.on('finish', common.mustCall(() => {
        assert.strictEqual(readFileSync(dest, 'utf8'), 'before reopen\n');
      }));
      stream.end();
    }));
  }));
}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  let retryCallCount = 0;
  const err = new Error('EAGAIN');
  err.code = 'EAGAIN';
  let reportError = true;

  const fsOverride = {
    writeSync: common.mustCall((...args) => {
      if (reportError) {
        reportError = false;
        throw err;
      }
      return writeSync(...args);
    }, 2),
  };

  const stream = new Utf8Stream({
    fd,
    sync: false,
    minLength: 1000,
    retryEAGAIN: common.mustCall((err, writeBufferLen, remainingBufferLen) => {
      retryCallCount++;
      assert.strictEqual(err.code, 'EAGAIN');
      assert.strictEqual(writeBufferLen, 12);
      assert.strictEqual(remainingBufferLen, 0);
      return false; // Don't retry
    }),
    fs: fsOverride,
  });

  stream.on('ready', common.mustCall(() => {
    assert.ok(stream.write('hello world\n'));
    assert.throws(() => stream.flushSync(), err);
    assert.ok(stream.write('something else\n'));
    stream.flushSync();
    stream.end();

    stream.on('finish', common.mustCall(() => {
      readFile(dest, 'utf8', common.mustSucceed((data) => {
        assert.strictEqual(data, 'hello world\nsomething else\n');
        assert.strictEqual(retryCallCount, 1);
      }));
    }));
  }));
}
