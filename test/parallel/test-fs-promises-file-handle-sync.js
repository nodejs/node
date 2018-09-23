'use strict';
require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

const { access, copyFile, open } = require('fs').promises;
const path = require('path');

async function validateSync() {
  tmpdir.refresh();
  const dest = path.resolve(tmpdir.path, 'baz.js');
  await copyFile(fixtures.path('baz.js'), dest);
  await access(dest, 'r');
  const handle = await open(dest, 'r+');
  await handle.datasync();
  await handle.sync();
  const buf = Buffer.from('hello world');
  await handle.write(buf);
  const ret = await handle.read(Buffer.alloc(11), 0, 11, 0);
  assert.strictEqual(ret.bytesRead, 11);
  assert.deepStrictEqual(ret.buffer, buf);
}

validateSync();
