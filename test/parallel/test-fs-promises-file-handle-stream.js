'use strict';

const common = require('../common');

// The following tests validate base functionality for the fs.promises
// FileHandle.write method.

const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { finished } = require('stream/promises');
const { buffer } = require('stream/consumers');
const tmpDir = tmpdir.path;

tmpdir.refresh();

async function validateWrite() {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-write.txt');
  const fileHandle = await open(filePathForHandle, 'w');
  const buffer = Buffer.from('Hello world'.repeat(100), 'utf8');

  const stream = fileHandle.createWriteStream();
  stream.end(buffer);
  await finished(stream);

  const readFileData = fs.readFileSync(filePathForHandle);
  assert.deepStrictEqual(buffer, readFileData);
}

async function validateRead() {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-read.txt');
  const buf = Buffer.from('Hello world'.repeat(100), 'utf8');

  fs.writeFileSync(filePathForHandle, buf);

  const fileHandle = await open(filePathForHandle);
  assert.deepStrictEqual(
    await buffer(fileHandle.createReadStream()),
    buf
  );
}

async function validateReadStreamReleasesFileHandleCloseListener() {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-read-listener.txt');
  const buf = Buffer.from('Hello world', 'utf8');

  fs.writeFileSync(filePathForHandle, buf);

  const fileHandle = await open(filePathForHandle);

  for (let i = 0; i < buf.length; i++) {
    await buffer(fileHandle.createReadStream({
      start: i,
      end: i,
      autoClose: false,
    }));

    assert.strictEqual(fileHandle.listenerCount('close'), 0);
  }

  await fileHandle.close();
}

async function validateWriteStreamReleasesFileHandleCloseListener() {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-write-listener.txt');
  const buf = Buffer.from('Hello world', 'utf8');

  const fileHandle = await open(filePathForHandle, 'w');

  for (let i = 0; i < buf.length; i++) {
    const stream = fileHandle.createWriteStream({
      start: i,
      autoClose: false,
    });
    stream.end(buf.subarray(i, i + 1));
    await finished(stream);

    assert.strictEqual(fileHandle.listenerCount('close'), 0);
  }

  await fileHandle.close();
  assert.deepStrictEqual(fs.readFileSync(filePathForHandle), buf);
}

async function validateReadStreamAutoCloseClosesFileHandle() {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-read-auto-close.txt');
  const buf = Buffer.from('Hello world', 'utf8');

  fs.writeFileSync(filePathForHandle, buf);

  const fileHandle = await open(filePathForHandle);
  const closed = new Promise((resolve) => {
    fileHandle.once('close', common.mustCall(resolve));
  });

  assert.deepStrictEqual(await buffer(fileHandle.createReadStream()), buf);
  await closed;
  assert.strictEqual(fileHandle.listenerCount('close'), 0);
}

async function validateWriteStreamAutoCloseClosesFileHandle() {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-write-auto-close.txt');
  const buf = Buffer.from('Hello world', 'utf8');

  const fileHandle = await open(filePathForHandle, 'w');
  const closed = new Promise((resolve) => {
    fileHandle.once('close', common.mustCall(resolve));
  });
  const stream = fileHandle.createWriteStream();

  stream.end(buf);
  await finished(stream);
  await closed;

  assert.strictEqual(fileHandle.listenerCount('close'), 0);
  assert.deepStrictEqual(fs.readFileSync(filePathForHandle), buf);
}

Promise.all([
  validateWrite(),
  validateRead(),
  validateReadStreamReleasesFileHandleCloseListener(),
  validateWriteStreamReleasesFileHandleCloseListener(),
  validateReadStreamAutoCloseClosesFileHandle(),
  validateWriteStreamAutoCloseClosesFileHandle(),
]).then(common.mustCall());
