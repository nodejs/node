'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const { open, readFile } = require('fs').promises;
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

async function validateTruncate() {
  const text = 'Hello world';
  const filename = path.resolve(tmpdir.path, 'truncate-file.txt');
  const fileHandle = await open(filename, 'w+');

  const buffer = Buffer.from(text, 'utf8');
  await fileHandle.write(buffer, 0, buffer.length);

  assert.deepStrictEqual((await readFile(filename)).toString(), text);

  await fileHandle.truncate(5);
  assert.deepStrictEqual((await readFile(filename)).toString(), 'Hello');
}

validateTruncate().then(common.mustCall());
