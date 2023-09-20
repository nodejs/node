'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

function handler(err, folder) {
  assert.ifError(err);
  assert(fs.existsSync(folder));
  assert.strictEqual(this, undefined);
}

// Test with plain string
{
  const tmpFolder = fs.mkdtempSync(tmpdir.resolve('foo.'));

  assert.strictEqual(path.basename(tmpFolder).length, 'foo.XXXXXX'.length);
  assert(fs.existsSync(tmpFolder));

  const utf8 = fs.mkdtempSync(tmpdir.resolve('\u0222abc.'));
  assert.strictEqual(Buffer.byteLength(path.basename(utf8)),
                     Buffer.byteLength('\u0222abc.XXXXXX'));
  assert(fs.existsSync(utf8));

  fs.mkdtemp(tmpdir.resolve('bar.'), common.mustCall(handler));

  // Same test as above, but making sure that passing an options object doesn't
  // affect the way the callback function is handled.
  fs.mkdtemp(tmpdir.resolve('bar.'), {}, common.mustCall(handler));

  const warningMsg = 'mkdtemp() templates ending with X are not portable. ' +
                     'For details see: https://nodejs.org/api/fs.html';
  common.expectWarning('Warning', warningMsg);
  fs.mkdtemp(tmpdir.resolve('bar.X'), common.mustCall(handler));
}

// Test with URL object
{
  const tmpFolder = fs.mkdtempSync(tmpdir.fileURL('foo.'));

  assert.strictEqual(path.basename(tmpFolder).length, 'foo.XXXXXX'.length);
  assert(fs.existsSync(tmpFolder));

  const utf8 = fs.mkdtempSync(tmpdir.fileURL('\u0222abc.'));
  assert.strictEqual(Buffer.byteLength(path.basename(utf8)),
                     Buffer.byteLength('\u0222abc.XXXXXX'));
  assert(fs.existsSync(utf8));

  fs.mkdtemp(tmpdir.fileURL('bar.'), common.mustCall(handler));

  // Same test as above, but making sure that passing an options object doesn't
  // affect the way the callback function is handled.
  fs.mkdtemp(tmpdir.fileURL('bar.'), {}, common.mustCall(handler));

  // Warning fires only once
  fs.mkdtemp(tmpdir.fileURL('bar.X'), common.mustCall(handler));
}

// Test with Buffer
{
  const tmpFolder = fs.mkdtempSync(Buffer.from(tmpdir.resolve('foo.')));

  assert.strictEqual(path.basename(tmpFolder).length, 'foo.XXXXXX'.length);
  assert(fs.existsSync(tmpFolder));

  const utf8 = fs.mkdtempSync(Buffer.from(tmpdir.resolve('\u0222abc.')));
  assert.strictEqual(Buffer.byteLength(path.basename(utf8)),
                     Buffer.byteLength('\u0222abc.XXXXXX'));
  assert(fs.existsSync(utf8));

  fs.mkdtemp(Buffer.from(tmpdir.resolve('bar.')), common.mustCall(handler));

  // Same test as above, but making sure that passing an options object doesn't
  // affect the way the callback function is handled.
  fs.mkdtemp(Buffer.from(tmpdir.resolve('bar.')), {}, common.mustCall(handler));

  // Warning fires only once
  fs.mkdtemp(Buffer.from(tmpdir.resolve('bar.X')), common.mustCall(handler));
}

// Test with Uint8Array
{
  const encoder = new TextEncoder();

  const tmpFolder = fs.mkdtempSync(encoder.encode(tmpdir.resolve('foo.')));

  assert.strictEqual(path.basename(tmpFolder).length, 'foo.XXXXXX'.length);
  assert(fs.existsSync(tmpFolder));

  const utf8 = fs.mkdtempSync(encoder.encode(tmpdir.resolve('\u0222abc.')));
  assert.strictEqual(Buffer.byteLength(path.basename(utf8)),
                     Buffer.byteLength('\u0222abc.XXXXXX'));
  assert(fs.existsSync(utf8));

  fs.mkdtemp(encoder.encode(tmpdir.resolve('bar.')), common.mustCall(handler));

  // Same test as above, but making sure that passing an options object doesn't
  // affect the way the callback function is handled.
  fs.mkdtemp(encoder.encode(tmpdir.resolve('bar.')), {}, common.mustCall(handler));

  // Warning fires only once
  fs.mkdtemp(encoder.encode(tmpdir.resolve('bar.X')), common.mustCall(handler));
}
