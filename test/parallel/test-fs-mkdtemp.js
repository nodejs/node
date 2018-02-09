'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const tmpFolder = fs.mkdtempSync(path.join(tmpdir.path, 'foo.'));

assert.strictEqual(path.basename(tmpFolder).length, 'foo.XXXXXX'.length);
assert(common.fileExists(tmpFolder));

const utf8 = fs.mkdtempSync(path.join(tmpdir.path, '\u0222abc.'));
assert.strictEqual(Buffer.byteLength(path.basename(utf8)),
                   Buffer.byteLength('\u0222abc.XXXXXX'));
assert(common.fileExists(utf8));

function handler(err, folder) {
  assert.ifError(err);
  assert(common.fileExists(folder));
  assert.strictEqual(this, undefined);
}

fs.mkdtemp(path.join(tmpdir.path, 'bar.'), common.mustCall(handler));

// Same test as above, but making sure that passing an options object doesn't
// affect the way the callback function is handled.
fs.mkdtemp(path.join(tmpdir.path, 'bar.'), {}, common.mustCall(handler));

// Making sure that not passing a callback doesn't crash, as a default function
// is passed internally.
fs.mkdtemp(path.join(tmpdir.path, 'bar-'));
fs.mkdtemp(path.join(tmpdir.path, 'bar-'), {});
