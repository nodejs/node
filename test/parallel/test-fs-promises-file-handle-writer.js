'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const tmpDir = tmpdir.path;

// =============================================================================
// Basic write()
// =============================================================================

async function testBasicWrite() {
  const filePath = path.join(tmpDir, 'writer-basic.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();
  await w.write(Buffer.from('Hello '));
  await w.write(Buffer.from('World!'));
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, 12);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'Hello World!');
}

// =============================================================================
// Basic writev()
// =============================================================================

async function testBasicWritev() {
  const filePath = path.join(tmpDir, 'writer-writev.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();
  await w.writev([
    Buffer.from('aaa'),
    Buffer.from('bbb'),
    Buffer.from('ccc'),
  ]);
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, 9);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'aaabbbccc');
}

// =============================================================================
// Mixed write() and writev()
// =============================================================================

async function testMixedWriteAndWritev() {
  const filePath = path.join(tmpDir, 'writer-mixed.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();
  await w.write(Buffer.from('head-'));
  await w.writev([Buffer.from('mid1-'), Buffer.from('mid2-')]);
  await w.write(Buffer.from('tail'));
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, 19);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'head-mid1-mid2-tail');
}

// =============================================================================
// end() returns totalBytesWritten
// =============================================================================

async function testEndReturnsTotalBytes() {
  const filePath = path.join(tmpDir, 'writer-totalbytes.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  // Write some data in various sizes
  const sizes = [100, 200, 300, 400, 500];
  let expected = 0;
  for (const size of sizes) {
    await w.write(Buffer.alloc(size, 0x41));
    expected += size;
  }
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, expected);
  assert.strictEqual(totalBytes, 1500);
  assert.strictEqual(fs.statSync(filePath).size, 1500);
}

// =============================================================================
// write() with string input (UTF-8 encoding)
// =============================================================================

async function testWriteString() {
  const filePath = path.join(tmpDir, 'writer-string.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();
  await w.write('Hello ');
  await w.write('World!');
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, 12);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'Hello World!');
}

// =============================================================================
// write() with string containing multi-byte UTF-8 characters
// =============================================================================

async function testWriteStringMultibyte() {
  const filePath = path.join(tmpDir, 'writer-string-multibyte.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();
  const input = 'café ☕ 日本語';
  await w.write(input);
  const totalBytes = await w.end();
  await fh.close();

  const expected = Buffer.from(input, 'utf8');
  assert.strictEqual(totalBytes, expected.byteLength);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), input);
}

// =============================================================================
// writev() with string chunks (UTF-8 encoding)
// =============================================================================

async function testWritevStrings() {
  const filePath = path.join(tmpDir, 'writer-writev-strings.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();
  await w.writev(['aaa', 'bbb', 'ccc']);
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, 9);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'aaabbbccc');
}

// =============================================================================
// writev() with mixed string and Uint8Array chunks
// =============================================================================

async function testWritevMixed() {
  const filePath = path.join(tmpDir, 'writer-writev-mixed.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();
  await w.writev(['hello', Buffer.from(' '), 'world']);
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, 11);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'hello world');
}

Promise.all([
  testBasicWrite(),
  testBasicWritev(),
  testMixedWriteAndWritev(),
  testEndReturnsTotalBytes(),
  testWriteString(),
  testWriteStringMultibyte(),
  testWritevStrings(),
  testWritevMixed(),
]).then(common.mustCall());
