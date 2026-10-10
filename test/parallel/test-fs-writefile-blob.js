'use strict';

const common = require('../common');
const fs = require('fs');
const fsPromises = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { Blob } = require('buffer');

tmpdir.refresh();

const dest = path.resolve(tmpdir.path, 'tmp.txt');
const otherDest = path.resolve(tmpdir.path, 'tmp-2.txt');

const text = 'Hello, Node.js Core!';
const textBlob = new Blob([text]);

// A binary blob whose content is not valid UTF-8, to ensure binary data is
// preserved verbatim and not corrupted by any encoding handling.
const binaryInput = Buffer.from(
  Array.from({ length: 1024 }, (_, i) => i % 256));
const binaryBlob = new Blob([binaryInput]);

// A blob larger than the internal writeFile chunk size so that the chunked
// write loop inside writeFileHandle() is exercised.
const largeString = 'dogs running'.repeat(64 * 1024);
const largeBlob = new Blob([largeString]);

// An empty blob.
const emptyBlob = new Blob([]);

async function doWriteFileBlob() {
  await fsPromises.writeFile(dest, textBlob);
  const data = fs.readFileSync(dest, 'utf8');
  assert.strictEqual(data, text);
}

async function doWriteFileEmptyBlob() {
  await fsPromises.writeFile(dest, emptyBlob);
  const data = fs.readFileSync(dest);
  assert.strictEqual(data.length, 0);
}

async function doWriteFileBinaryBlob() {
  await fsPromises.writeFile(dest, binaryBlob);
  const data = fs.readFileSync(dest);
  assert.deepStrictEqual(data, binaryInput);
}

async function doWriteFileLargeBlob() {
  await fsPromises.writeFile(dest, largeBlob);
  const data = fs.readFileSync(dest, 'utf8');
  assert.strictEqual(data, largeString);
}

async function doAppendFileBlob() {
  await fsPromises.writeFile(dest, textBlob);
  await fsPromises.appendFile(dest, new Blob([' appended']));
  const data = fs.readFileSync(dest, 'utf8');
  assert.strictEqual(data, `${text} appended`);
}

async function doFileHandleWriteFileBlob() {
  const handle = await fsPromises.open(dest, 'w+');
  try {
    await handle.writeFile(textBlob);
    const data = fs.readFileSync(dest, 'utf8');
    assert.strictEqual(data, text);
  } finally {
    await handle.close();
  }
}

async function doFileHandleAppendFileBlob() {
  const handle = await fsPromises.open(dest, 'w+');
  try {
    await handle.writeFile(new Blob(['hello']));
    await handle.appendFile(new Blob([' world']));
    const data = fs.readFileSync(dest, 'utf8');
    assert.strictEqual(data, 'hello world');
  } finally {
    await handle.close();
  }
}

// Binary data must be preserved even when an encoding option is provided,
// because Blob chunks are already typed arrays and the encoding is ignored.
async function doWriteFileBinaryBlobWithEncoding() {
  await fsPromises.writeFile(dest, binaryBlob, 'utf8');
  const data = fs.readFileSync(dest);
  assert.deepStrictEqual(data, binaryInput);
}

async function doWriteBlobWithCancel() {
  const controller = new AbortController();
  const { signal } = controller;
  process.nextTick(() => controller.abort());
  await assert.rejects(
    fsPromises.writeFile(otherDest, textBlob, { signal }),
    { name: 'AbortError' }
  );
}

(async () => {
  await doWriteFileBlob();
  await doWriteFileEmptyBlob();
  await doWriteFileBinaryBlob();
  await doWriteFileLargeBlob();
  await doAppendFileBlob();
  await doFileHandleWriteFileBlob();
  await doFileHandleAppendFileBlob();
  await doWriteFileBinaryBlobWithEncoding();
  await doWriteBlobWithCancel();
})().then(common.mustCall());
