'use strict';

require('../common');
const { test } = require('node:test');
const tmpdir = require('../common/tmpdir');
const {
  ok,
  strictEqual,
  throws,
} = require('node:assert');
const {
  openSync,
  closeSync,
  readFile,
  readSync,
  readFileSync,
  writeSync,
  stat,
  mkdirSync,
} = require('node:fs');
const { Utf8Stream } = require('node:fs');
const { join } = require('node:path');
const { isMainThread } = require('node:worker_threads');
const { once } = require('node:events');

tmpdir.refresh();
let fileCounter = 0;
if (isMainThread) {
  process.umask(0o000);
}

function getTempFile() {
  const testDir = join(tmpdir.path, `test-${process.pid}-${process.hrtime.bigint()}`);
  mkdirSync(testDir, { recursive: true });
  return join(testDir, `fastutf8stream-${fileCounter++}.log`);
}

function createClosePromise(stream) {
  return once(stream, 'close');
}


test('Utf8Stream sync with mocked writeSync retries', async (t) => {
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  let callCount = 0;

  const stream = new Utf8Stream({
    fd,
    minLength: 0,
    sync: true,
    fs: {
      writeSync: (...args) => {
        if (callCount++ === 0) return 0;
        writeSync(...args);
      },
    }
  });

  const closePromise = createClosePromise(stream);

  await new Promise((resolve) => {
    stream.on('ready', () => {
      ok(stream.write('hello world\n'));
      ok(stream.write('something else\n'));

      stream.end();

      stream.on('finish', () => {
        readFile(dest, 'utf8', (err, data) => {
          t.assert.ifError(err);
          strictEqual(data, 'hello world\nsomething else\n');
          resolve();
        });
      });
    });
  });

  await closePromise;

});

test('Utf8Stream async with mocked writeSync retries', async (t) => {
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  let callCount = 0;

  const stream = new Utf8Stream({
    fd,
    minLength: 100,
    sync: false,
    fs: {
      writeSync: (...args) => {
        if (callCount++ === 0) return 0;
        return writeSync(...args);
      },
    }
  });

  const closePromise = createClosePromise(stream);

  await new Promise((resolve) => {
    stream.on('ready', () => {
      ok(stream.write('hello world\n'));
      ok(stream.write('something else\n'));
      stream.flushSync();
      stream.on('write', t.assert.fail);
      stream.end();
      stream.on('finish', () => {
        readFile(dest, 'utf8', (err, data) => {
          t.assert.ifError(err);
          strictEqual(data, 'hello world\nsomething else\n');
          resolve();
        });
      });
    });
  });

  await closePromise;

});

test('Utf8Stream sync with drain event', async (t) => {
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  const stream = new Utf8Stream({
    fd,
    minLength: 0,
    sync: true,
    fs: {
      writeSync,
    }
  });

  const closePromise = createClosePromise(stream);

  ok(stream.write('hello world\n'));
  ok(stream.write('something else\n'));

  await new Promise((resolve) => {
    stream.on('drain', () => {
      const data = readFileSync(dest, 'utf8');
      strictEqual(data, 'hello world\nsomething else\n');
      stream.end();
      resolve();
    });
  });

  await closePromise;

});

test('Utf8Stream sync large data write', async (t) => {
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  const stream = new Utf8Stream({ fd, minLength: 0, sync: true });

  const closePromise = createClosePromise(stream);

  const buf = Buffer.alloc(1024).fill('x').toString();
  let length = 0;

  for (let i = 0; i < 1024; i++) {
    length += buf.length;
    stream.write(buf);
  }

  stream.flushSync();
  stream.end();

  await closePromise;

  // Now that the stream is closed, we can safely stat the file
  const stat_result = await new Promise((resolve, reject) => {
    stat(dest, (err, stat) => {
      if (err) reject(err);
      else resolve(stat);
    });
  });

  strictEqual(stat_result.size, length);

});

test('Utf8Stream sync with emoji', async (t) => {
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  const stream = new Utf8Stream({ fd, minLength: 0, sync: true });

  const closePromise = createClosePromise(stream);

  let buf = Buffer.alloc((1024 * 16) - 2).fill('x');
  const length = buf.length + 4;
  buf = buf.toString() + '🌲';

  stream.write(buf);
  stream.flushSync();
  stream.end();

  await closePromise;

  // Now that the stream is closed, we can safely stat and read the file
  const stat_result = await new Promise((resolve, reject) => {
    stat(dest, (err, stat) => {
      if (err) reject(err);
      else resolve(stat);
    });
  });

  strictEqual(stat_result.size, length);
  const char = Buffer.alloc(4);
  const readFd = openSync(dest, 'r');
  readSync(readFd, char, 0, 4, length - 4);
  closeSync(readFd);
  strictEqual(char.toString(), '🌲');

});

test('Utf8Stream sync with dest path', async (t) => {
  const dest = getTempFile();

  const stream = new Utf8Stream({ dest, sync: true });

  const closePromise = createClosePromise(stream);

  ok(stream.write('hello world\n'));
  ok(stream.write('something else\n'));
  stream.flushSync();
  stream.end();

  await closePromise;

});

test('Utf8Stream constructor throws with invalid path', (t) => {
  throws(() => {
    new Utf8Stream({ dest: '/path/to/nowhere', sync: true });
  }, /ENOENT|EACCES/);
});

test('Utf8Stream sync with unicode characters', async (t) => {
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  const stream = new Utf8Stream({ fd, sync: true });

  const closePromise = createClosePromise(stream);

  ok(stream.write('hello world 👀\n'));
  ok(stream.write('another line 👀\n'));

  ok(true, 'writes completed successfully');

  stream.end();

  await closePromise;

});

test('Utf8Stream sync with multiple unicode writes', async (t) => {
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  const stream = new Utf8Stream({ fd, sync: true, minLength: 20 });

  const closePromise = createClosePromise(stream);

  let str = '';
  for (let i = 0; i < 20; i++) {
    ok(stream.write('👀'));
    str += '👀';
  }

  ok(true, 'writes completed successfully');

  await new Promise((resolve) => {
    readFile(dest, 'utf8', (err, data) => {
      t.assert.ifError(err);
      strictEqual(data, str);
      stream.end();
      resolve();
    });
  });

  await closePromise;

});
