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

it('fsync with sync', async (t) => {

  const originalFsyncSync = fs.fsyncSync;
  fs.fsyncSync = common.mustCall(originalFsyncSync, 2);

  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: true, fsync: true });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  const data = fs.readFileSync(dest, 'utf8');
  t.assert.strictEqual(data, 'hello world\nsomething else\n');
  fs.fsyncSync = originalFsyncSync;
});

it('fsync with async', async (t) => {

  const originalFsyncSync = fs.fsyncSync;
  fs.fsyncSync = common.mustCall(originalFsyncSync, 2);

  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, fsync: true });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  stream.on('finish', common.mustCall(() => {
    fs.readFile(dest, 'utf8', common.mustSucceed((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
    }));
  }));

  const { promise, resolve } = Promise.withResolvers();
  stream.on('close', common.mustCall(resolve));

  await promise;

  fs.fsyncSync = originalFsyncSync;
});
