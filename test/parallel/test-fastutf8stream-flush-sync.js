'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
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

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  stream.flushSync();

  setImmediate(common.mustCall(() => {
    stream.end();
    const data = readFileSync(dest, 'utf8');
    assert.strictEqual(data, 'hello world\nsomething else\n');
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
    minLength: 1000,
    fs: fsOverride,
  });

  stream.on('ready', common.mustCall(() => {
    assert.ok(stream.write('hello world\n'));
    assert.ok(stream.write('something else\n'));
    stream.flushSync();
    stream.end();

    stream.on('finish', common.mustCall(() => {
      readFile(dest, 'utf8', common.mustSucceed((data) => {
        assert.strictEqual(data, 'hello world\nsomething else\n');
      }));
    }));
  }));
}

for (const contentMode of ['utf8', 'buffer']) {
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  const stream = new Utf8Stream({
    contentMode,
    fd,
    minLength: 0,
    sync: false,
  });
  const text = `${contentMode} asynchronous write\n`;
  const data = contentMode === 'buffer' ? Buffer.from(text) : text;

  stream.on('ready', common.mustCall(() => {
    assert.ok(stream.write(data));
    assert.throws(
      () => stream.flushSync(),
      { code: 'ERR_INVALID_STATE' },
    );
    stream.flush(common.mustSucceed(() => {
      stream.end();
      readFile(dest, 'utf8', common.mustSucceed((contents) => {
        assert.strictEqual(contents, text);
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
    retryEAGAIN: common.mustCall((err, writeBufferLen, remainingBufferLen) => {
      retryCallCount++;
      assert.strictEqual(err.code, 'EAGAIN');
      assert.strictEqual(writeBufferLen, 12);
      assert.strictEqual(remainingBufferLen, 0);
      return false; // Don't retry
    }),
    fs: fsOverride,
  });

  stream.on('ready', common.mustCall(() => {
    assert.ok(stream.write('hello world\n'));
    assert.throws(() => stream.flushSync(), err);
    assert.ok(stream.write('something else\n'));
    stream.flushSync();
    stream.end();

    stream.on('finish', common.mustCall(() => {
      readFile(dest, 'utf8', common.mustSucceed((data) => {
        assert.strictEqual(data, 'hello world\nsomething else\n');
        assert.strictEqual(retryCallCount, 1);
      }));
    }));
  }));
}
