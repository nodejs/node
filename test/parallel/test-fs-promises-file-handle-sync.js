'use strict';
require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

const { access, copyFile, open } = require('fs').promises;

async function validate() {
  tmpdir.refresh();
  const dest = tmpdir.resolve('baz.js');
  await assert.rejects(
    copyFile(fixtures.path('baz.js'), dest, 'r'),
    {
      code: 'ERR_INVALID_ARG_TYPE',
    }
  );
  await copyFile(fixtures.path('baz.js'), dest);
  await assert.rejects(
    access(dest, 'r'),
    { code: 'ERR_INVALID_ARG_TYPE', message: /mode/ }
  );
  await access(dest);
  const handle = await open(dest, 'r+');
  await handle.datasync();
  await handle.sync();
  const buf = Buffer.from('hello world');
  await handle.write(buf);
  const ret = await handle.read(Buffer.alloc(11), 0, 11, 0);
  assert.strictEqual(ret.bytesRead, 11);
  assert.deepStrictEqual(ret.buffer, buf);
  await handle.close();
}

validate();
