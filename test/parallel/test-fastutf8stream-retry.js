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

it('retry on EAGAIN sync', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');

  const originalWriteSync = fs.writeSync;
  let callCount = 0;

  fs.writeSync = function(fd, buf, enc) {
    callCount++;
    if (callCount === 1) {
      const err = new Error('EAGAIN');
      err.code = 'EAGAIN';
      throw err;
    }
    return originalWriteSync.call(fs, fd, buf, enc);
  };

  const stream = new FastUtf8Stream({ fd, sync: true, minLength: 0 });

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

it('retry on EAGAIN', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');

  const originalWrite = fs.write;
  let callCount = 0;

  fs.write = function(fd, buf, ...args) {
    callCount++;
    if (callCount === 1) {
      const err = new Error('EAGAIN');
      err.code = 'EAGAIN';
      const callback = args[args.length - 1];
      process.nextTick(callback, err);
      return;
    }
    return originalWrite.call(fs, fd, buf, ...args);
  };

  const stream = new FastUtf8Stream({ fd, sync: false, minLength: 0 });

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

it('emit error on async EAGAIN', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');

  const originalWrite = fs.write;
  let callCount = 0;

  fs.write = function(fd, buf, ...args) {
    callCount++;
    if (callCount === 1) {
      const err = new Error('EAGAIN');
      err.code = 'EAGAIN';
      const callback = args[args.length - 1];
      process.nextTick(callback, err);
      return;
    }
    return originalWrite.call(fs, fd, buf, ...args);
  };

  const stream = new FastUtf8Stream({
    fd,
    sync: false,
    minLength: 12,
    retryEAGAIN: (err, writeBufferLen, remainingBufferLen) => {
      t.assert.strictEqual(err.code, 'EAGAIN');
      t.assert.strictEqual(writeBufferLen, 12);
      t.assert.strictEqual(remainingBufferLen, 0);
      return false; // Don't retry
    }
  });

  stream.on('ready', common.mustCall());

  stream.once('error', common.mustCall((err) => {
    t.assert.strictEqual(err.code, 'EAGAIN');
    t.assert.ok(stream.write('something else\n'));
  }));

  t.assert.ok(stream.write('hello world\n'));

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

it('retry on EBUSY', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');

  const originalWrite = fs.write;
  let callCount = 0;

  fs.write = function(fd, buf, ...args) {
    callCount++;
    if (callCount === 1) {
      const err = new Error('EBUSY');
      err.code = 'EBUSY';
      const callback = args[args.length - 1];
      process.nextTick(callback, err);
      return;
    }
    return originalWrite.call(fs, fd, buf, ...args);
  };

  const stream = new FastUtf8Stream({ fd, sync: false, minLength: 0 });

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

it('emit error on async EBUSY', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');

  const originalWrite = fs.write;
  let callCount = 0;

  fs.write = function(fd, buf, ...args) {
    callCount++;
    if (callCount === 1) {
      const err = new Error('EBUSY');
      err.code = 'EBUSY';
      const callback = args[args.length - 1];
      process.nextTick(callback, err);
      return;
    }
    return originalWrite.call(fs, fd, buf, ...args);
  };

  const stream = new FastUtf8Stream({
    fd,
    sync: false,
    minLength: 12,
    retryEAGAIN: (err, writeBufferLen, remainingBufferLen) => {
      t.assert.strictEqual(err.code, 'EBUSY');
      t.assert.strictEqual(writeBufferLen, 12);
      t.assert.strictEqual(remainingBufferLen, 0);
      return false; // Don't retry
    }
  });

  stream.on('ready', common.mustCall());

  stream.once('error', common.mustCall((err) => {
    t.assert.strictEqual(err.code, 'EBUSY');
    t.assert.ok(stream.write('something else\n'));
  }));

  t.assert.ok(stream.write('hello world\n'));

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
