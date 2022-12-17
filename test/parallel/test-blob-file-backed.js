'use strict';

const common = require('../common');
const { strictEqual } = require('assert');
const { open } = require('fs/promises');
const { TextEncoder } = require('util');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');
const testfile = path.join(tmpdir.path, 'test.txt');

tmpdir.refresh();

const data = `${'a'.repeat(1000)}${'b'.repeat(2000)}`;

fs.writeFileSync(testfile, data);

(async () => {

  const fh = await open(testfile);
  const blob = fh.blob();

  const ab = await blob.arrayBuffer();
  const dec = new TextDecoder();

  strictEqual(dec.decode(new Uint8Array(ab)), data);
  strictEqual(await blob.text(), data);

  let stream = blob.stream();
  let check = '';
  for await (const chunk of stream)
    check = dec.decode(chunk);
  strictEqual(check, data);

  // If the file is modified tho, the stream errors.
  fs.writeFileSync(testfile, data + 'abc');

  stream = blob.stream();
  try {
    for await (const chunk of stream) {}
  } catch (err) {
    strictEqual(err.message, 'read EINVAL');
    strictEqual(err.code, 'EINVAL');
  }

})().then(common.mustCall());
