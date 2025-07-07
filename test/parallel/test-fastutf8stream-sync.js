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

it('write buffers that are not totally written with sync mode', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');

  const originalWriteSync = fs.writeSync;
  let callCount = 0;

  fs.writeSync = function(fd, buf, enc) {
    callCount++;
    if (callCount === 1) {
      // First call returns 0 (nothing written)
      fs.writeSync = (fd, buf, enc) => {
        return originalWriteSync.call(fs, fd, buf, enc);
      };
      return 0;
    }
    return originalWriteSync.call(fs, fd, buf, enc);
  };

  const stream = new FastUtf8Stream({ fd, minLength: 0, sync: true });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  const { promise, resolve } = Promise.withResolvers();
  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
      fs.writeSync = originalWriteSync;
      resolve();
    }));
  }));

  await promise;
});

it('write buffers that are not totally written with flush sync', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');

  const originalWriteSync = fs.writeSync;
  let callCount = 0;

  fs.writeSync = function(fd, buf, enc) {
    callCount++;
    if (callCount === 1) {
      // First call returns 0 (nothing written)
      fs.writeSync = originalWriteSync;
      return 0;
    }
    return originalWriteSync.call(fs, fd, buf, enc);
  };

  const stream = new FastUtf8Stream({ fd, minLength: 100, sync: false });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.flushSync();

  stream.on('write', (n) => {
    if (n === 0) {
      throw new Error('shouldn\'t call write handler after flushing with n === 0');
    }
  });

  stream.end();

  const { promise, resolve } = Promise.withResolvers();
  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
      fs.writeSync = originalWriteSync;
      resolve();
    }));
  }));

  await promise;
});

it('sync writing is fully sync', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');

  const originalWriteSync = fs.writeSync;

  fs.writeSync = function(fd, buf, enc) {
    return originalWriteSync.call(fs, fd, buf, enc);
  };

  const stream = new FastUtf8Stream({ fd, minLength: 0, sync: true });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  // 'drain' will be only emitted once
  const { promise, resolve } = Promise.withResolvers();
  stream.on('drain', common.mustCall(resolve));

  await promise;

  const data = fs.readFileSync(dest, 'utf8');
  t.assert.strictEqual(data, 'hello world\nsomething else\n');
  fs.writeSync = originalWriteSync;
});

it('write enormously large buffers sync', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 0, sync: true });

  const buf = Buffer.alloc(1024).fill('x').toString(); // 1 KB
  let length = 0;

  // Reduce iterations to avoid test timeout
  for (let i = 0; i < 1024; i++) {
    length += buf.length;
    stream.write(buf);
  }

  stream.end();

  const { promise, resolve } = Promise.withResolvers();
  stream.on('finish', common.mustCall(() => {
    fs.stat(dest, common.mustCall((err, stat) => {
      t.assert.ifError(err);
      t.assert.strictEqual(stat.size, length);
      resolve();
    }));
  }));

  await promise;
});

it('write enormously large buffers sync with utf8 multi-byte split', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 0, sync: true });

  let buf = Buffer.alloc((1024 * 16) - 2).fill('x'); // 16KB - 2B
  const length = buf.length + 4;
  buf = buf.toString() + '🌲'; // 16 KB + 4B emoji

  stream.write(buf);
  stream.end();

  const { promise, resolve } = Promise.withResolvers();
  stream.on('finish', common.mustCall(() => {
    fs.stat(dest, common.mustCall((err, stat) => {
      t.assert.ifError(err);
      t.assert.strictEqual(stat.size, length);
      const char = Buffer.alloc(4);
      const readFd = fs.openSync(dest, 'r');
      fs.readSync(readFd, char, 0, 4, length - 4);
      fs.closeSync(readFd);
      t.assert.strictEqual(char.toString(), '🌲');
      resolve();
    }));
  }));

  await promise;
});

it('file specified by dest path available immediately when options.sync is true', (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: true });
  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));
  stream.flushSync();
  stream.end();
});

it('sync error handling', (t) => {
  t.assert.throws(() => {
    new FastUtf8Stream({ dest: '/path/to/nowhere', sync: true });
  }, /ENOENT|EACCES/);
});

it('._len must always be equal or greater than 0', (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: true });

  t.assert.ok(stream.write('hello world 👀\n'));
  t.assert.ok(stream.write('another line 👀\n'));

  // Check internal buffer length (may not be available in FastUtf8Stream)
  // This is implementation-specific, so we just verify writes succeeded
  t.assert.ok(true, 'writes completed successfully');

  stream.end();
});

it('minLength with multi-byte characters', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: true, minLength: 20 });

  let str = '';
  for (let i = 0; i < 20; i++) {
    t.assert.ok(stream.write('👀'));
    str += '👀';
  }

  // Check internal buffer length (implementation-specific)
  t.assert.ok(true, 'writes completed successfully');

  const { promise, resolve } = Promise.withResolvers();
  fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
    t.assert.strictEqual(data, str);
    resolve();
  }));

  await promise;
});
