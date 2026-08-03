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
} = require('node:fs');
const { Utf8Stream } = require('node:fs');
const { isMainThread } = require('node:worker_threads');
const { join } = require('node:path');

let fileCounter = 0;
tmpdir.refresh();

if (isMainThread) {
  process.umask(0o000);
}

function getTempFile() {
  return join(tmpdir.path, `fastutf8stream-${process.pid}-${Date.now()}-${fileCounter++}.log`);
}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  const stream = new Utf8Stream({ fd, sync: false });

  // Test successful write
  ok(stream.write('hello world\n'));
  stream.destroy();

  throws(() => stream.write('hello world\n'), Error);

  readFile(dest, 'utf8', common.mustSucceed((data) => {
    strictEqual(data, 'hello world\n');
  }));

  stream.on('finish', common.mustNotCall());
  stream.on('close', common.mustCall());
};

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');
  const stream = new Utf8Stream({ fd, sync: true });

  ok(stream.write('hello world\n'));
  stream.destroy();
  throws(() => stream.write('hello world\n'), Error);

  const data = readFileSync(dest, 'utf8');
  strictEqual(data, 'hello world\n');
};

{
  const dest = getTempFile();
  const stream = new Utf8Stream({ dest });
  stream.destroy();
  stream.on('close', common.mustCall());
}
