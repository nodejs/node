'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const {
  open,
  openSync,
  readFile,
  writeFileSync,
  write,
  writeSync,
} = require('node:fs');
const { join } = require('node:path');
const { Utf8Stream } = require('node:fs');
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

// Flush cb is invoked when flushing before 'ready' while the stream is
// still opening (async mode only; sync mode writes synchronously).
{
  const dest = getTempFile();

  const stream = new Utf8Stream({
    dest,
    minLength: 4096,
    sync: false,
    fs: {
      open(file, flags, mode, cb) {
        process.nextTick(() => {
          assert.ok(stream.write('hello world\n'));
          stream.flush(common.mustSucceed(() => {
            stream.destroy();
          }));
          open(file, flags, mode, cb);
        });
      },
    },
  });

  stream.on('ready', common.mustCall());
}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  const stream = new Utf8Stream({ fd, minLength: 10, sync: false });
  let flushed = false;

  assert.ok(stream.write('asynchronous flush\n'));
  assert.ok(stream.write('queued\n'));
  assert.strictEqual(stream.writing, true);
  stream.flush(common.mustSucceed(() => {
    flushed = true;
    assert.strictEqual(stream.writing, false);
    readFile(dest, 'utf8', common.mustSucceed((data) => {
      assert.strictEqual(data, 'asynchronous flush\nqueued\n');
      stream.end();
    }));
  }));
  assert.strictEqual(flushed, false);
}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  const stream = new Utf8Stream({ fd, minLength: 0, sync: false });

  assert.ok(stream.write('flush before end\n'));
  stream.flush(common.mustSucceed());
  stream.on('finish', common.mustCall(() => {
    readFile(dest, 'utf8', common.mustSucceed((data) => {
      assert.strictEqual(data, 'flush before end\n');
    }));
  }));
  stream.end();
}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  const writeError = new Error('write failed');
  const stream = new Utf8Stream({
    fd,
    minLength: 0,
    sync: false,
    fs: {
      write: common.mustCall((_fd, _data, _encoding, callback) => {
        process.nextTick(callback, writeError);
      }),
    },
  });

  stream.on('error', common.mustCall((error) => {
    assert.strictEqual(error, writeError);
  }));
  assert.ok(stream.write('failed write before end\n'));
  stream.flush(common.mustCall((error) => {
    assert.strictEqual(error, writeError);
  }));
  stream.end();
}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  let fsyncCalls = 0;
  const stream = new Utf8Stream({
    fd,
    minLength: 10,
    sync: false,
    fs: {
      fsync: common.mustCall((_fd, callback) => {
        fsyncCalls++;
        if (fsyncCalls === 1) {
          assert.ok(stream.write('late\n'));
        }
        process.nextTick(callback);
      }, 2),
    },
  });

  assert.ok(stream.write('initial write\n'));
  stream.flush(common.mustSucceed());
  stream.on('finish', common.mustCall(() => {
    readFile(dest, 'utf8', common.mustSucceed((data) => {
      assert.strictEqual(data, 'initial write\nlate\n');
    }));
  }));
  stream.end();
}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  const flushError = new Error('flush failed');
  let fsyncCalls = 0;
  const stream = new Utf8Stream({
    fd,
    minLength: 0,
    sync: false,
    fs: {
      fsync: common.mustCall((_fd, callback) => {
        fsyncCalls++;
        process.nextTick(callback, fsyncCalls === 1 ? flushError : null);
      }, 2),
    },
  });

  assert.ok(stream.write('failed flush before end\n'));
  stream.flush(common.mustCall((error) => {
    assert.strictEqual(error, flushError);
  }));
  stream.on('close', common.mustCall());
  stream.end();
}

{
  const dest = getTempFile();
  const stream = new Utf8Stream({
    dest,
    fs: {
      open(...args) {
        setImmediate(() => open(...args));
      },
    },
  });

  stream.flush(common.mustSucceed());
  stream.on('close', common.mustCall());
  stream.end();
}

{
  const dest = getTempFile();
  const stream = new Utf8Stream({
    dest,
    fs: {
      open(...args) {
        setImmediate(() => open(...args));
      },
    },
  });

  stream.flush(common.mustCall((error) => {
    assert.strictEqual(error?.code, 'ERR_INVALID_STATE');
  }));
  stream.on('close', common.mustCall());
  stream.destroy();
  stream.flush(common.mustCall((error) => {
    assert.strictEqual(error?.code, 'ERR_INVALID_STATE');
  }));
}

{
  const dest = getTempFile();
  const stream = new Utf8Stream({
    dest,
    fs: {
      open(...args) {
        setImmediate(() => open(...args));
      },
    },
  });

  stream.on('close', common.mustCall());
  stream.end();
  stream.destroy();
}

function runTests(sync) {
  {
    const dest = getTempFile();
    writeFileSync(dest, 'hello world\n');
    const stream = new Utf8Stream({ dest, append: false, sync });

    stream.on('ready', common.mustCall(() => {
      assert.ok(stream.write('something else\n'));
      stream.flush();

      stream.on('drain', common.mustCall(() => {
        readFile(dest, 'utf8', common.mustSucceed((data) => {
          assert.strictEqual(data, 'something else\n');
          stream.end();
        }));
      }));
    }));
  }

  {
    const dest = join(getTempFile(), 'out.log');
    const stream = new Utf8Stream({ dest, mkdir: true, sync });

    stream.on('ready', common.mustCall(() => {
      assert.ok(stream.write('hello world\n'));
      stream.flush();

      stream.on('drain', common.mustCall(() => {
        readFile(dest, 'utf8', common.mustSucceed((data) => {
          assert.strictEqual(data, 'hello world\n');
          stream.end();
        }));
      }));
    }));
  }

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');
    const stream = new Utf8Stream({ fd, minLength: 4096, sync });

    stream.on('ready', common.mustCall(() => {
      assert.ok(stream.write('hello world\n'));
      assert.ok(stream.write('something else\n'));
      stream.flush();

      stream.on('drain', common.mustCall(() => {
        readFile(dest, 'utf8', common.mustSucceed((data) => {
          assert.strictEqual(data, 'hello world\nsomething else\n');
          stream.end();
        }));
      }));
    }));
  }

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');
    const stream = new Utf8Stream({ fd, minLength: 4096, sync });

    stream.on('ready', common.mustCall(() => {
      stream.flush();
      stream.on('drain', common.mustCall(() => {
        stream.end();
      }));
    }));
  }

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');
    const stream = new Utf8Stream({ fd, minLength: 4096, sync });

    stream.on('ready', common.mustCall(() => {
      assert.ok(stream.write('hello world\n'));
      assert.ok(stream.write('something else\n'));

      stream.flush(common.mustSucceed(() => {
        stream.end();
      }));
    }));
  }

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');
    const stream = new Utf8Stream({ fd, minLength: 4096, sync });
    stream.on('ready', common.mustCall(() => {
      stream.flush(common.mustSucceed(() => {
        stream.end();
      }));
    }));
  }

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');
    const stream = new Utf8Stream({ fd, minLength: 0, sync });

    stream.flush(common.mustSucceed(() => {
      stream.end();
    }));
  }

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');
    const stream = new Utf8Stream({ fd, minLength: 4096, sync });
    stream.destroy();
    stream.flush(common.mustCall(assert.ok));
  }

  {
    // Flush cb is invoked with the error when the underlying write fails.
    const dest = getTempFile();
    const fd = openSync(dest, 'w');

    const err = new Error('other');
    err.code = 'other';
    let first = true;

    const fsOverride = {};
    if (sync) {
      fsOverride.writeSync = common.mustCallAtLeast((...args) => {
        if (first) {
          first = false;
          throw err;
        }
        return writeSync(...args);
      }, 1);
    } else {
      fsOverride.write = common.mustCallAtLeast((...args) => {
        const callback = args[args.length - 1];
        if (first) {
          first = false;
          process.nextTick(callback, err);
          return;
        }
        return write(...args);
      }, 1);
    }

    const stream = new Utf8Stream({
      fd,
      sync,
      minLength: 4096,
      fs: fsOverride,
    });

    stream.on('ready', common.mustCall(() => {
      assert.ok(stream.write('hello world\n'));
      stream.flush(common.mustCall((e) => {
        assert.strictEqual(e.code, 'other');
        stream.destroy();
      }));
    }));
  }

  {
    // Flush cb is invoked once the in-flight write completes.
    const dest = getTempFile();
    const fd = openSync(dest, 'w');

    const fsOverride = {};
    if (sync) {
      fsOverride.writeSync = common.mustCallAtLeast((...args) => {
        stream.flush(common.mustSucceed(() => {
          stream.destroy();
        }));
        return writeSync(...args);
      }, 1);
    } else {
      fsOverride.write = common.mustCallAtLeast((...args) => {
        stream.flush(common.mustSucceed(() => {
          stream.destroy();
        }));
        return write(...args);
      }, 1);
    }

    const stream = new Utf8Stream({
      fd,
      sync,
      minLength: 1,
      fs: fsOverride,
    });

    stream.on('ready', common.mustCall(() => {
      assert.ok(stream.write('hello world\n'));
    }));
  }
}
