'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const {
  open,
  openSync,
  readFile,
  writeFileSync,
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
}
