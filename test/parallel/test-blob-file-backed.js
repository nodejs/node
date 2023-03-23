'use strict';

const common = require('../common');
const {
  strictEqual,
  rejects,
} = require('assert');
const { TextDecoder } = require('util');
const {
  writeFileSync,
  openAsBlob,
} = require('fs');

const {
  unlink
} = require('fs/promises');
const path = require('path');
const { Blob } = require('buffer');

const tmpdir = require('../common/tmpdir');
const testfile = path.join(tmpdir.path, 'test-file-backed-blob.txt');
const testfile2 = path.join(tmpdir.path, 'test-file-backed-blob2.txt');
const testfile3 = path.join(tmpdir.path, 'test-file-backed-blob3.txt');
tmpdir.refresh();

const data = `${'a'.repeat(1000)}${'b'.repeat(2000)}`;

writeFileSync(testfile, data);
writeFileSync(testfile2, data.repeat(100));
writeFileSync(testfile3, '');

(async () => {
  const blob = await openAsBlob(testfile);

  const ab = await blob.arrayBuffer();
  const dec = new TextDecoder();

  strictEqual(dec.decode(new Uint8Array(ab)), data);
  strictEqual(await blob.text(), data);

  // Can be read multiple times
  let stream = blob.stream();
  let check = '';
  for await (const chunk of stream)
    check = dec.decode(chunk);
  strictEqual(check, data);

  // Can be combined with other Blob's and read
  const combined = new Blob(['hello', blob, 'world']);
  const ab2 = await combined.arrayBuffer();
  strictEqual(dec.decode(ab2.slice(0, 5)), 'hello');
  strictEqual(dec.decode(ab2.slice(5, -5)), data);
  strictEqual(dec.decode(ab2.slice(-5)), 'world');

  // If the file is modified tho, the stream errors.
  writeFileSync(testfile, data + 'abc');

  stream = blob.stream();

  const read = async () => {
    // eslint-disable-next-line no-unused-vars, no-empty
    for await (const _ of stream) {}
  };

  await rejects(read(), { name: 'NotReadableError' });

  await unlink(testfile);
})().then(common.mustCall());

(async () => {
  const blob = await openAsBlob(testfile2);
  const stream = blob.stream();
  const read = async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream) {
      writeFileSync(testfile2, data + 'abc');
    }
  };

  await rejects(read(), { name: 'NotReadableError' });

  await unlink(testfile2);
})().then(common.mustCall());

(async () => {
  const blob = await openAsBlob(testfile3);
  strictEqual(blob.size, 0);
  strictEqual(await blob.text(), '');
  writeFileSync(testfile3, 'abc');
  await rejects(blob.text(), { name: 'NotReadableError' });
  await unlink(testfile3);
})().then(common.mustCall());

(async () => {
  const blob = await openAsBlob(testfile3);
  strictEqual(blob.size, 0);
  writeFileSync(testfile3, 'abc');
  const stream = blob.stream();
  const reader = stream.getReader();
  await rejects(() => reader.read(), { name: 'NotReadableError' });
})().then(common.mustCall());
