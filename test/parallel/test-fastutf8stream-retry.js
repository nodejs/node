'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const {
  openSync,
  writeSync,
  write,
  readFile,
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
  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');

    const fsOverride = {};
    let errOnNext = true;
    const err = new Error('EAGAIN');
    err.code = 'EAGAIN';
    if (sync) {
      fsOverride.writeSync = common.mustCall((...args) => {
        if (errOnNext) {
          errOnNext = false;
          throw err;
        }
        writeSync(...args);
      }, 3);
    } else {
      fsOverride.write = common.mustCall((...args) => {
        if (errOnNext) {
          errOnNext = false;
          const callback = args[args.length - 1];
          process.nextTick(callback, err);
          return;
        }
        write(...args);
      }, 3);
    }

    const stream = new Utf8Stream({
      fd,
      sync,
      minLength: 0,
      fs: fsOverride,
    });

    stream.on('ready', common.mustCall(() => {
      assert.ok(stream.write('hello world\n'));
      assert.ok(stream.write('something else\n'));
      stream.end();
      stream.on('finish', common.mustCall(() => {
        readFile(dest, 'utf8', common.mustSucceed((data) => {
          assert.strictEqual(data, 'hello world\nsomething else\n');
        }));
      }));
    }));
  }
}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  const err = new Error('EAGAIN');
  err.code = 'EAGAIN';
  let errorOnNext = true;

  const stream = new Utf8Stream({
    fd,
    sync: false,
    minLength: 12,
    retryEAGAIN: common.mustCall((err, writeBufferLen, remainingBufferLen) => {
      assert.strictEqual(err.code, 'EAGAIN');
      assert.strictEqual(writeBufferLen, 12);
      assert.strictEqual(remainingBufferLen, 0);
      return false; // Don't retry
    }),
    fs: {
      write: common.mustCall((...args) => {
        if (errorOnNext) {
          errorOnNext = false;
          const callback = args[args.length - 1];
          process.nextTick(callback, err);
          return;
        }
        write(...args);
      }, 3),
    }
  });

  stream.on('ready', common.mustCall(() => {
    stream.once('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'EAGAIN');
      assert.ok(stream.write('something else\n'));
    }));

    assert.ok(stream.write('hello world\n'));
    stream.end();

    stream.on('finish', common.mustCall(() => {
      readFile(dest, 'utf8', common.mustSucceed((data) => {
        assert.strictEqual(data, 'hello world\nsomething else\n');
      }));
    }));
  }));
}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  const err = new Error('EBUSY');
  err.code = 'EBUSY';
  let errorOnNext = true;

  const stream = new Utf8Stream({
    fd,
    sync: false,
    minLength: 0,
    fs: {
      write: common.mustCall((...args) => {
        if (errorOnNext) {
          errorOnNext = false;
          const callback = args[args.length - 1];
          process.nextTick(callback, err);
          return;
        }
        write(...args);
      }, 3),
    }
  });

  stream.on('ready', common.mustCall(() => {
    assert.ok(stream.write('hello world\n'));
    assert.ok(stream.write('something else\n'));

    stream.end();

    stream.on('finish', common.mustCall(() => {
      readFile(dest, 'utf8', common.mustSucceed((data) => {
        assert.strictEqual(data, 'hello world\nsomething else\n');
      }));
    }));
  }));
}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  const err = new Error('EBUSY');
  err.code = 'EBUSY';
  let errorOnNext = true;

  const stream = new Utf8Stream({
    fd,
    sync: false,
    minLength: 12,
    retryEAGAIN: common.mustCall((err, writeBufferLen, remainingBufferLen) => {
      assert.strictEqual(err.code, 'EBUSY');
      assert.strictEqual(writeBufferLen, 12);
      assert.strictEqual(remainingBufferLen, 0);
      return false; // Don't retry
    }),
    fs: {
      write: common.mustCall((...args) => {
        if (errorOnNext) {
          errorOnNext = false;
          const callback = args[args.length - 1];
          process.nextTick(callback, err);
          return;
        }
        write(...args);
      }, 3),
    }
  });

  stream.on('ready', common.mustCall(() => {
    stream.once('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'EBUSY');
      assert.ok(stream.write('something else\n'));
    }));

    assert.ok(stream.write('hello world\n'));

    stream.end();

    stream.on('finish', common.mustCall(() => {
      readFile(dest, 'utf8', common.mustSucceed((data) => {
        assert.strictEqual(data, 'hello world\nsomething else\n');
      }));
    }));
  }));
}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  const err = new Error('EAGAIN');
  err.code = 'EAGAIN';
  let attempts = 0;

  const stream = new Utf8Stream({
    fd,
    sync: false,
    minLength: 0,
    maxWriteRetries: 3,
    // retryEAGAIN always returns true ("keep going"), so maxWriteRetries must
    // itself cap the number of attempts instead of retrying forever.
    retryEAGAIN: () => true,
    fs: {
      write: common.mustCall((...args) => {
        attempts++;
        const callback = args[args.length - 1];
        process.nextTick(callback, err);
      }, 4),
    }
  });

  stream.on('ready', common.mustCall(() => {
    assert.ok(stream.write('hello world\n'));
  }));

  stream.once('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'EAGAIN');
    // 1 initial attempt + 3 retries = 4 total fs.write calls, then give up.
    assert.strictEqual(attempts, 4);
    assert.strictEqual(stream.writing, false);
    stream.destroy();
  }));
}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  const err = new Error('EAGAIN');
  err.code = 'EAGAIN';

  const stream = new Utf8Stream({
    fd,
    sync: true,
    minLength: 0,
    maxWriteRetries: 3,
    retryEAGAIN: () => true,
    fs: {
      writeSync: common.mustCall((...args) => {
        throw err;
      }, 4),
    }
  });

  stream.on('ready', common.mustCall(() => {
    // Once retries are exhausted, write() must surface the error instead of
    // spinning forever on EAGAIN.
    assert.throws(() => {
      stream.write('hello world\n');
    }, (e) => e.code === 'EAGAIN');
    stream.destroy();
  }));
}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  const err = new Error('EAGAIN');
  err.code = 'EAGAIN';
  let call = 0;

  const stream = new Utf8Stream({
    fd,
    sync: false,
    minLength: 0,
    maxWriteRetries: 3,
    retryEAGAIN: () => true,
    fs: {
      write: common.mustCallAtLeast((...args) => {
        call++;
        const callback = args[args.length - 1];
        if (call % 3 === 0) {
          return write(...args);
        }
        process.nextTick(callback, err);
      }, 5),
    }
  });

  stream.on('error', common.mustNotCall());

  stream.on('ready', common.mustCall(() => {
    // First burst: calls 1-2 fail with EAGAIN, call 3 succeeds.
    assert.ok(stream.write('hello world\n'));
    stream.once('drain', common.mustCall(() => {
      // Second burst: calls 4-5 fail again. The counter must have been reset
      // by the successful write at call 3, so this burst succeeds too.
      stream.write('sonic boom\n');
      stream.end();
    }));
  }));

  stream.on('finish', common.mustCall(() => {
    readFile(dest, 'utf8', common.mustSucceed((data) => {
      assert.strictEqual(data, 'hello world\nsonic boom\n');
    }));
  }));
}
