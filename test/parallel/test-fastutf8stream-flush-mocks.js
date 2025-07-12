// Flags: --expose-internals
'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const { it } = require('node:test');
const fs = require('fs');
const path = require('path');
const FastUtf8Stream = require('internal/streams/fast-utf8-stream');

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

it('only call fsyncSync and not fsync when fsync: true sync', async (t) => {

  const originalFsync = fs.fsync;
  const originalFsyncSync = fs.fsyncSync;
  const originalWriteSync = fs.writeSync;

  fs.fsync = common.mustNotCall();
  fs.fsyncSync = common.mustCall();

  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({
    fd,
    sync: true,
    fsync: true,
    minLength: 4096
  });

  stream.on('ready', common.mustCall());

  fs.writeSync = common.mustCall((...args) => originalWriteSync(...args));

  t.assert.ok(stream.write('hello world\n'));

  const { promise, resolve } = Promise.withResolvers();
  stream.flush(common.mustSucceed(() => {
    process.nextTick(common.mustCall(resolve));
  }));

  await promise;

  fs.writeSync = originalWriteSync;
  fs.fsync = originalFsync;
  fs.fsyncSync = originalFsyncSync;
});

it('only call fsyncSync and not fsync when fsync: true', async (t) => {

  const originalFsync = fs.fsync;
  const originalFsyncSync = fs.fsyncSync;
  const originalWrite = fs.write;

  fs.fsync = common.mustNotCall();
  fs.fsyncSync = common.mustCall();

  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({
    fd,
    sync: false,
    fsync: true,
    minLength: 4096
  });

  stream.on('ready', common.mustCall());

  fs.write = common.mustCall((...args) => originalWrite(...args));

  t.assert.ok(stream.write('hello world\n'));

  const { promise, resolve } = Promise.withResolvers();
  stream.flush(common.mustSucceed(() => {
    process.nextTick(common.mustCall(resolve));
  }));

  await promise;

  fs.write = originalWrite;
  fs.fsync = originalFsync;
  fs.fsyncSync = originalFsyncSync;
});

it('call flush cb with error when fsync failed sync', async (t) => {

  const originalFsync = fs.fsync;
  const originalWriteSync = fs.writeSync;

  const err = new Error('boom');
  fs.fsync = common.mustCall((...args) => {
    args[args.length - 1](err);
  });

  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({
    fd,
    sync: true,
    minLength: 4096
  });

  stream.on('ready', common.mustCall());

  fs.writeSync = common.mustCall((...args) => originalWriteSync(...args));

  t.assert.ok(stream.write('hello world\n'));

  const { promise, resolve } = Promise.withResolvers();
  stream.flush(common.mustCall((actual) => {
    t.assert.strictEqual(actual, err);
    resolve();
  }));

  await promise;

  fs.writeSync = originalWriteSync;
  fs.fsync = originalFsync;
});


it('call flush cb with an error when failed to flush sync', async (t) => {

  const originalWriteSync = fs.writeSync;
  const err = new Error('boom');
  fs.writeSync = common.mustCall(() => {
    throw err;
  });

  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({
    fd,
    sync: true,
    minLength: 4096
  });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  const { promise, resolve } = Promise.withResolvers();
  stream.flush(common.mustCall((actual) => {
    t.assert.strictEqual(actual, err);
    fs.writeSync = originalWriteSync;
  }));

  stream.end();

  stream.on('close', common.mustCall(resolve));

  await promise;

});

it('call flush cb when finish writing when currently in the middle sync', async (t) => {

  const originalWriteSync = fs.writeSync;

  const { promise, resolve } = Promise.withResolvers();

  fs.writeSync = common.mustCall((...args) => {
    stream.flush(common.mustSucceed(resolve));
    originalWriteSync(...args);
  });

  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({
    fd,
    sync: true,

    // To trigger write without calling flush
    minLength: 1
  });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));

  await promise;

  fs.writeSync = originalWriteSync;
});

it('call flush cb when writing and trying to flush before ready (on async)', async (t) => {

  const originalOpen = fs.open;

  const { promise, resolve } = Promise.withResolvers();
  fs.open = function(...args) {
    process.nextTick(common.mustCall(() => {
      // Try writing and flushing before ready and in the middle of opening
      t.assert.ok(stream.write('hello world\n'));
      // calling flush
      stream.flush(common.mustSucceed(resolve));
      originalOpen(...args);
    }));
  };

  const dest = file();
  const stream = new FastUtf8Stream({
    fd: dest,
    // Only async as sync is part of the constructor so the user will not be able to call write/flush
    // before ready
    sync: false,

    // To not trigger write without calling flush
    minLength: 4096
  });

  stream.on('ready', common.mustCall());

  await promise;

  fs.open = originalOpen;
});
