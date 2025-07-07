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

it('flushSync sync', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: true });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.flushSync();

  const { promise, resolve } = Promise.withResolvers();

  // Let the file system settle down things
  setImmediate(common.mustCall(() => {
    stream.end();
    const data = fs.readFileSync(dest, 'utf8');
    t.assert.strictEqual(data, 'hello world\nsomething else\n');
    stream.on('close', common.mustCall(resolve));
  }));
  await promise;
});

it('flushSync', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: false });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.flushSync();

  const { promise, resolve } = Promise.withResolvers();

  // Let the file system settle down things
  setImmediate(common.mustCall(() => {
    stream.end();
    const data = fs.readFileSync(dest, 'utf8');
    t.assert.strictEqual(data, 'hello world\nsomething else\n');
    stream.on('close', common.mustCall(resolve));
  }));
  await promise;
});
