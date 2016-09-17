'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const Buffer = require('buffer').Buffer;

common.refreshTmpDir();

const tmpFolder = fs.mkdtempSync(path.join(common.tmpDir, 'foo.'));

assert.strictEqual(path.basename(tmpFolder).length, 'foo.XXXXXX'.length);
assert(common.fileExists(tmpFolder));

const utf8 = fs.mkdtempSync(path.join(common.tmpDir, '\u0222abc.'));
assert.equal(Buffer.byteLength(path.basename(utf8)),
             Buffer.byteLength('\u0222abc.XXXXXX'));
assert(common.fileExists(utf8));

function handler(err, folder) {
  assert.ifError(err);
  assert(common.fileExists(folder));
  assert.strictEqual(this, null);
}

fs.mkdtemp(path.join(common.tmpDir, 'bar.'), common.mustCall(handler));

// Same test as above, but making sure that passing an options object doesn't
// affect the way the callback function is handled.
fs.mkdtemp(path.join(common.tmpDir, 'bar.'), {}, common.mustCall(handler));

// Making sure that not passing a callback doesn't crash, as a default function
// is passed internally.
assert.doesNotThrow(() => fs.mkdtemp(path.join(common.tmpDir, 'bar-')));
assert.doesNotThrow(() => fs.mkdtemp(path.join(common.tmpDir, 'bar-'), {}));
