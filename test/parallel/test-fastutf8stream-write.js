// Flags: --expose-internals
'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const path = require('path');
const FastUtf8Stream = require('internal/streams/fast-utf8-stream');
const { it } = require('node:test');

tmpdir.refresh();
process.umask(0o000);

const files = [];
let count = 0;

function file() {
  const file = path.join(tmpdir.path,
                         `sonic-boom-${process.pid}-${process.hrtime().toString()}-${count++}`);
  files.push(file);
  return file;
}

it('write things to a file descriptor sync', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: true });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  const { promise, resolve } = Promise.withResolvers();
  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
      resolve();
    }));
  }));
  stream.on('close', common.mustCall());

  await promise;
});

it('write things to a file descriptor', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: false });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  const { promise, resolve } = Promise.withResolvers();
  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
      resolve();
    }));
  }));
  stream.on('close', common.mustCall());

  await promise;
});

it('write things in a streaming fashion sync', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: true });

  stream.once('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\n');
      t.assert.ok(stream.write('something else\n'));
    }));

    stream.once('drain', common.mustCall(() => {
      fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
        t.assert.strictEqual(data, 'hello world\nsomething else\n');
        stream.end();
      }));
    }));
  }));

  t.assert.ok(stream.write('hello world\n'));

  const { promise, resolve } = Promise.withResolvers();

  stream.on('finish', common.mustCall());
  stream.on('close', common.mustCall(resolve));

  await promise;
});

it('write things in a streaming fashion', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: false });

  stream.once('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\n');
      t.assert.ok(stream.write('something else\n'));
    }));

    stream.once('drain', common.mustCall(() => {
      fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
        t.assert.strictEqual(data, 'hello world\nsomething else\n');
        stream.end();
      }));
    }));
  }));

  t.assert.ok(stream.write('hello world\n'));

  const { promise, resolve } = Promise.withResolvers();

  stream.on('finish', common.mustCall());
  stream.on('close', common.mustCall(resolve));

  await promise;
});

it('can be piped into sync', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: true });
  const source = fs.createReadStream(__filename, { encoding: 'utf8' });

  source.pipe(stream);

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(__filename, 'utf8').then(common.mustCall((expected) => {
      fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
        t.assert.strictEqual(data, expected);
      }));
    }));
  }));

  const { promise, resolve } = Promise.withResolvers();
  stream.on('close', common.mustCall(resolve));

  await promise;
});

it('can be piped into', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: false });
  const source = fs.createReadStream(__filename, { encoding: 'utf8' });

  source.pipe(stream);

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(__filename, 'utf8').then(common.mustCall((expected) => {
      fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
        t.assert.strictEqual(data, expected);
      }));
    }));
  }));

  const { promise, resolve } = Promise.withResolvers();
  stream.on('close', common.mustCall(resolve));

  await promise;
});

it('write things to a file sync', async (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: true });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  const { promise, resolve } = Promise.withResolvers();

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
    }));
  }));
  stream.on('close', common.mustCall(resolve));

  await promise;
});

it('write things to a file', async (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: false });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  const { promise, resolve } = Promise.withResolvers();

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
    }));
  }));
  stream.on('close', common.mustCall(resolve));

  await promise;
});

it('emit write events sync', async (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: true });

  stream.on('ready', common.mustCall());

  let length = 0;
  stream.on('write', (bytes) => {
    length += bytes;
  });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  const { promise, resolve } = Promise.withResolvers();

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
      t.assert.strictEqual(length, 27);
      resolve();
    }));
  }));

  await promise;
});

it('emit write events', async (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: false });

  stream.on('ready', common.mustCall());

  let length = 0;
  stream.on('write', (bytes) => {
    length += bytes;
  });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  const { promise, resolve } = Promise.withResolvers();

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
      t.assert.strictEqual(length, 27);
      resolve();
    }));
  }));

  await promise;
});

it('write buffers that are not totally written async', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');

  const originalWrite = fs.write;
  let callCount = 0;

  fs.write = function(fd, buf, ...args) {
    callCount++;
    if (callCount === 1) {
      // First call returns 0 (nothing written)
      fs.write = function(fd, buf, ...args) {
        return originalWrite.call(fs, fd, buf, ...args);
      };
      const callback = args[args.length - 1];
      process.nextTick(callback, null, 0);
      return;
    }
    return originalWrite.call(fs, fd, buf, ...args);
  };

  const stream = new FastUtf8Stream({ fd, minLength: 0, sync: false });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  const { promise, resolve } = Promise.withResolvers();
  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
      fs.write = originalWrite;
      resolve();
    }));
  }));

  await promise;
});

it('write enormously large buffers async', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 0, sync: false });

  const buf = Buffer.alloc(1024).fill('x').toString(); // 1 KB
  let length = 0;

  // Reduce iterations to avoid test timeout
  for (let i = 0; i < 1024; i++) {
    length += buf.length;
    stream.write(buf);
  }

  stream.end();

  stream.on('finish', common.mustCall(() => {
    fs.stat(dest, common.mustCall((err, stat) => {
      t.assert.ifError(err);
      t.assert.strictEqual(stat.size, length);
    }));
  }));

  const { promise, resolve } = Promise.withResolvers();

  stream.on('close', common.mustCall(resolve));
  await promise;
});

it('make sure `maxLength` is passed', (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, maxLength: 65536 });
  t.assert.strictEqual(stream.maxLength, 65536);
  stream.end();
});
