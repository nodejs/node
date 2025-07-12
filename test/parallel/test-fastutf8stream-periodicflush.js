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

it('periodicflush off sync', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: true, minLength: 5000 });

  t.assert.ok(stream.write('hello world\n'));

  // Wait - without periodicFlush, data should not be written
  const { promise, resolve } = Promise.withResolvers();
  setTimeout(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, '');
      stream.destroy();
      resolve();
    }));
  }, 100);

  await promise;
});

it('periodicflush off', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: false, minLength: 5000 });

  t.assert.ok(stream.write('hello world\n'));

  // Wait - without periodicFlush, data should not be written
  const { promise, resolve } = Promise.withResolvers();
  setTimeout(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, '');
      stream.destroy();
      resolve();
    }));
  }, 100);

  await promise;
});
