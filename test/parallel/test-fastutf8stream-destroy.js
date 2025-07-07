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

it('destroy', async (t) => {
  t.plan(3);

  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: false });

  t.assert.ok(stream.write('hello world\n'));
  stream.destroy();
  t.assert.throws(() => { stream.write('hello world\n'); }, {
    code: 'ERR_INVALID_STATE',
  });

  const data = await fs.promises.readFile(dest, 'utf8');
  t.assert.strictEqual(data, 'hello world\n');

  stream.on('finish', common.mustNotCall());
  stream.on('close', common.mustCall());
});

it('destroy sync', async (t) => {
  t.plan(3);

  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: true });

  t.assert.ok(stream.write('hello world\n'));
  stream.destroy();
  t.assert.throws(() => { stream.write('hello world\n'); }, {
    code: 'ERR_INVALID_STATE',
  });

  const data = await fs.promises.readFile(dest, 'utf8');
  t.assert.strictEqual(data, 'hello world\n');

  stream.on('finish', common.mustNotCall());
  stream.on('close', common.mustCall());
});

it('destroy while opening', (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest });

  stream.destroy();
  stream.on('close', common.mustCall());
});
