'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const {
  ok,
  strictEqual,
} = require('node:assert');
const {
  fsyncSync,
  openSync,
  readFile,
  readFileSync,
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

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  const stream = new Utf8Stream({
    fd,
    sync: true,
    fsync: true,
    fs: {
      fsyncSync: common.mustCall(fsyncSync, 2),
    },
  });

  ok(stream.write('hello world\n'));
  ok(stream.write('something else\n'));
  stream.end();

  const data = readFileSync(dest, 'utf8');
  strictEqual(data, 'hello world\nsomething else\n');
}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  const stream = new Utf8Stream({
    fd,
    fsync: true,
    fs: {
      fsyncSync: common.mustCall(fsyncSync, 2),
    }
  });

  ok(stream.write('hello world\n'));
  ok(stream.write('something else\n'));
  stream.end();

  stream.on('finish', common.mustCall(() => {
    readFile(dest, 'utf8', common.mustSucceed((data) => {
      strictEqual(data, 'hello world\nsomething else\n');
    }));
  }));

  stream.on('close', common.mustCall());
}
