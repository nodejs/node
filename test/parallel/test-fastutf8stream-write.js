'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const {
  openSync,
  readFile,
  createReadStream,
  write,
  writeSync,
  stat,
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
    const stream = new Utf8Stream({ fd, sync });
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
    const stream = new Utf8Stream({ fd, sync });

    stream.once('drain', common.mustCall(() => {
      readFile(dest, 'utf8', common.mustSucceed((data) => {
        assert.strictEqual(data, 'hello world\n');
        assert.ok(stream.write('something else\n'));

        stream.once('drain', common.mustCall(() => {
          readFile(dest, 'utf8', common.mustSucceed((data) => {
            assert.strictEqual(data, 'hello world\nsomething else\n');
            stream.end();
          }));
        }));
      }));
    }));

    assert.ok(stream.write('hello world\n'));
  };

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');
    const stream = new Utf8Stream({ fd, sync });
    const source = createReadStream(__filename, { encoding: 'utf8' });

    source.pipe(stream);

    stream.on('finish', common.mustCall(() => {
      readFile(__filename, 'utf8', common.mustSucceed((expected) => {
        readFile(dest, 'utf8', common.mustSucceed((data) => {
          assert.strictEqual(data, expected);
        }));
      }));
    }));
  }

  {
    const dest = getTempFile();
    const stream = new Utf8Stream({ dest, sync });

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
    const stream = new Utf8Stream({ dest, minLength: 4096, sync });

    stream.on('ready', common.mustCall(() => {
      assert.ok(stream.write('hello world\n'));
      assert.ok(stream.write('something else\n'));

      stream.on('drain', common.mustNotCall());

      setTimeout(common.mustCall(() => {
        readFile(dest, 'utf8', common.mustSucceed((data) => {
          assert.strictEqual(data, ''); // Should be empty due to minLength

          stream.end();

          stream.on('finish', common.mustCall(() => {
            readFile(dest, 'utf8', common.mustSucceed((data) => {
              assert.strictEqual(data, 'hello world\nsomething else\n');
            }));
          }));
        }));
      }), 100);
    }));
  }

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');

    let throwOnNext = true;

    const fsOverride = {};
    if (sync) {
      fsOverride.writeSync = common.mustCall((...args) => {
        if (throwOnNext) {
          throw new Error('recoverable error');
        }
        return writeSync(...args);
      }, 3);
    } else {
      fsOverride.write = common.mustCall((...args) => {
        if (throwOnNext) {
          const callback = args[args.length - 1];
          process.nextTick(callback, new Error('recoverable error'));
          return;
        }
        return write(...args);
      }, 3);
    }

    const stream = new Utf8Stream({
      fd,
      minLength: 0,
      sync,
      fs: fsOverride,
    });

    stream.on('ready', common.mustCall(() => {
      stream.on('error', common.mustCall());
      assert.ok(stream.write('hello world\n'));

      setTimeout(common.mustCall(() => {
        throwOnNext = false;
        assert.ok(stream.write('something else\n'));
        stream.end();
        stream.on('finish', common.mustCall(() => {
          readFile(dest, 'utf8', common.mustSucceed((data) => {
            assert.strictEqual(data, 'hello world\nsomething else\n');
          }));
        }));
      }), 10);
    }));
  }

  {
    const dest = getTempFile();
    const stream = new Utf8Stream({ dest, sync });

    stream.on('ready', common.mustCall(() => {
      let length = 0;
      stream.on('write', (bytes) => {
        length += bytes;
      });

      assert.ok(stream.write('hello world\n'));
      assert.ok(stream.write('something else\n'));

      stream.end();

      stream.on('finish', common.mustCall(() => {
        readFile(dest, 'utf8', common.mustSucceed((data) => {
          assert.strictEqual(data, 'hello world\nsomething else\n');
          assert.strictEqual(length, 27);
        }));
      }));
    }));
  }
}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  let callCount = 0;

  const stream = new Utf8Stream({
    fd,
    minLength: 0,
    sync: false,
    fs: {
      write: common.mustCall((...args) => {
        if (callCount++ === 0) {
          const callback = args[args.length - 1];
          process.nextTick(callback, null, 0);
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
  const stream = new Utf8Stream({ fd, minLength: 0, sync: false });

  const buf = Buffer.alloc(1024).fill('x').toString(); // 1 KB
  let length = 0;

  // Reduce iterations to avoid test timeout
  for (let i = 0; i < 1024; i++) {
    length += buf.length;
    stream.write(buf);
  }

  stream.end();

  stream.on('finish', common.mustCall(() => {
    stat(dest, common.mustSucceed((stat) => {
      assert.strictEqual(stat.size, length);
    }));
  }));
}

{
  const dest = getTempFile();
  const stream = new Utf8Stream({ dest, maxLength: 65536 });
  assert.strictEqual(stream.maxLength, 65536);
  stream.end();
}

// maxLength drop behavior: writing past maxLength must emit 'drop' and
// never write the overflowing chunk (see SonicBoom write tests).
{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  const buf = Buffer.alloc(100).fill('x').toString();

  const stream = new Utf8Stream({
    fd,
    minLength: 101,
    maxLength: 102,
    sync: false,
    fs: {
      write: common.mustCall((...args) => {
        const data = args[1];
        const callback = args[args.length - 1];
        assert.strictEqual(data.length, buf.length + 2);
        process.nextTick(callback, null, data.length);
        stream.end();
      }, 1),
    }
  });

  stream.on('drop', common.mustNotCall());

  stream.on('ready', common.mustCall(() => {
    assert.ok(stream.write(buf));
    assert.ok(stream.write('aa'));
  }));
}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  const buf = Buffer.alloc(100).fill('x').toString();

  const stream = new Utf8Stream({
    fd,
    minLength: 101,
    maxLength: 102,
    sync: false,
    fs: {
      write: common.mustCall((...args) => {
        const data = args[1];
        const callback = args[args.length - 1];
        assert.strictEqual(data.length, buf.length);
        process.nextTick(callback, null, data.length);
      }, 1),
    }
  });

  stream.on('drop', common.mustCall((data) => {
    assert.strictEqual(data.length, 3);
    stream.end();
  }));

  stream.on('ready', common.mustCall(() => {
    assert.ok(stream.write(buf));
    assert.ok(stream.write('aaa'));
  }));
}
