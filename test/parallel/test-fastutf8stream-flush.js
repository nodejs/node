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

// flush cb is invoked when flushing before 'ready' while the stream is
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
          stream.flush(common.mustCall((e) => {
            assert.ifError(e);
            stream.destroy();
          }));
          open(file, flags, mode, cb);
        });
      },
    },
  });

  stream.on('ready', common.mustCall(() => {}));
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
    // flush cb is invoked with the error when the underlying write fails.
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
    // flush cb is invoked once the in-flight write completes.
    const dest = getTempFile();
    const fd = openSync(dest, 'w');

    const fsOverride = {};
    if (sync) {
      fsOverride.writeSync = common.mustCallAtLeast((...args) => {
        stream.flush(common.mustCall((e) => {
          assert.ifError(e);
          stream.destroy();
        }));
        return writeSync(...args);
      }, 1);
    } else {
      fsOverride.write = common.mustCallAtLeast((...args) => {
        const callback = args[args.length - 1];
        stream.flush(common.mustCall((e) => {
          assert.ifError(e);
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
