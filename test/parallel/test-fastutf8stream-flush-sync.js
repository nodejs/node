'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const {
  ok,
  strictEqual,
  throws,
} = require('node:assert');
const {
  openSync,
  readFile,
  readFileSync,
  writeSync,
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
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  const stream = new Utf8Stream({ fd, minLength: 4096, sync });

  ok(stream.write('hello world\n'));
  ok(stream.write('something else\n'));

  stream.flushSync();

  setImmediate(common.mustCall(() => {
    stream.end();
    const data = readFileSync(dest, 'utf8');
    strictEqual(data, 'hello world\nsomething else\n');
    stream.on('close', common.mustCall());
  }));
}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  let reportEagain = true;

  const fsOverride = {
    writeSync: common.mustCall((...args) => {
      if (reportEagain) {
        reportEagain = false;
        const err = new Error('EAGAIN');
        err.code = 'EAGAIN';
        throw err;
      }
      writeSync(...args);
    }, 2),
  };

  const stream = new Utf8Stream({
    fd,
    sync: false,
    minLength: 0,
    fs: fsOverride,
  });

  stream.on('ready', common.mustCall(() => {
    ok(stream.write('hello world\n'));
    ok(stream.write('something else\n'));
    stream.flushSync();
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

  let retryCallCount = 0;
  const err = new Error('EAGAIN');
  err.code = 'EAGAIN';
  let reportError = true;

  const fsOverride = {
    writeSync: common.mustCall((...args) => {
      if (reportError) {
        reportError = false;
        throw err;
      }
      return writeSync(...args);
    }, 2),
  };

  const stream = new Utf8Stream({
    fd,
    sync: false,
    minLength: 1000,
    retryEAGAIN: (err, writeBufferLen, remainingBufferLen) => {
      retryCallCount++;
      strictEqual(err.code, 'EAGAIN');
      strictEqual(writeBufferLen, 12);
      strictEqual(remainingBufferLen, 0);
      return false; // Don't retry
    },
    fs: fsOverride,
  });

  stream.on('ready', common.mustCall(() => {
    ok(stream.write('hello world\n'));
    throws(() => stream.flushSync(), err);
    ok(stream.write('something else\n'));
    stream.flushSync();
    stream.end();

    stream.on('finish', common.mustCall(() => {
      readFile(dest, 'utf8', common.mustSucceed((data) => {
        strictEqual(data, 'hello world\nsomething else\n');
        strictEqual(retryCallCount, 1);
      }));
    }));
  }));
}
