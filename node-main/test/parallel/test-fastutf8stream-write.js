'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const {
  ok,
  strictEqual,
} = require('node:assert');
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
      ok(stream.write('hello world\n'));
      ok(stream.write('something else\n'));

      stream.end();

      stream.on('finish', common.mustCall(() => {
        readFile(dest, 'utf8', common.mustSucceed((data) => {
          strictEqual(data, 'hello world\nsomething else\n');
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
        strictEqual(data, 'hello world\n');
        ok(stream.write('something else\n'));

        stream.once('drain', common.mustCall(() => {
          readFile(dest, 'utf8', common.mustSucceed((data) => {
            strictEqual(data, 'hello world\nsomething else\n');
            stream.end();
          }));
        }));
      }));
    }));

    ok(stream.write('hello world\n'));
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
          strictEqual(data, expected);
        }));
      }));
    }));
  }

  {
    const dest = getTempFile();
    const stream = new Utf8Stream({ dest, sync });

    stream.on('ready', common.mustCall(() => {
      ok(stream.write('hello world\n'));
      ok(stream.write('something else\n'));

      stream.end();

      stream.on('finish', common.mustCall(() => {
        readFile(dest, 'utf8', common.mustSucceed((data) => {
          strictEqual(data, 'hello world\nsomething else\n');
        }));
      }));
    }));
  }

  {
    const dest = getTempFile();
    const stream = new Utf8Stream({ dest, minLength: 4096, sync });

    stream.on('ready', common.mustCall(() => {
      ok(stream.write('hello world\n'));
      ok(stream.write('something else\n'));

      stream.on('drain', common.mustNotCall());

      setTimeout(common.mustCall(() => {
        readFile(dest, 'utf8', common.mustSucceed((data) => {
          strictEqual(data, ''); // Should be empty due to minLength

          stream.end();

          stream.on('finish', common.mustCall(() => {
            readFile(dest, 'utf8', common.mustSucceed((data) => {
              strictEqual(data, 'hello world\nsomething else\n');
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
      ok(stream.write('hello world\n'));

      setTimeout(common.mustCall(() => {
        throwOnNext = false;
        ok(stream.write('something else\n'));
        stream.end();
        stream.on('finish', common.mustCall(() => {
          readFile(dest, 'utf8', common.mustSucceed((data) => {
            strictEqual(data, 'hello world\nsomething else\n');
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

      ok(stream.write('hello world\n'));
      ok(stream.write('something else\n'));

      stream.end();

      stream.on('finish', common.mustCall(() => {
        readFile(dest, 'utf8', common.mustSucceed((data) => {
          strictEqual(data, 'hello world\nsomething else\n');
          strictEqual(length, 27);
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
    ok(stream.write('hello world\n'));
    ok(stream.write('something else\n'));

    stream.end();

    stream.on('finish', common.mustCall(() => {
      readFile(dest, 'utf8', common.mustSucceed((data) => {
        strictEqual(data, 'hello world\nsomething else\n');
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
      strictEqual(stat.size, length);
    }));
  }));
}

{
  const dest = getTempFile();
  const stream = new Utf8Stream({ dest, maxLength: 65536 });
  strictEqual(stream.maxLength, 65536);
  stream.end();
}
