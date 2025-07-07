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

const kMaxWrite = 16 * 1024;

it('drain deadlock', async (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: false, minLength: 9999 });

  t.assert.ok(stream.write(Buffer.alloc(1500).fill('x').toString()));
  t.assert.ok(stream.write(Buffer.alloc(1500).fill('x').toString()));
  t.assert.ok(!stream.write(Buffer.alloc(kMaxWrite).fill('x').toString()));
  const { promise, resolve } = Promise.withResolvers();
  stream.on('drain', common.mustCall(resolve));
  await promise;
});

it('should throw if minLength >= maxWrite', (t) => {
  t.assert.throws(() => {
    const dest = file();
    const fd = fs.openSync(dest, 'w');

    new FastUtf8Stream({
      fd,
      minLength: kMaxWrite
    });
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
  });
});
