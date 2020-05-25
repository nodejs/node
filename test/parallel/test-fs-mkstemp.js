'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const tmpFile = fs.mkstempSync(path.join(tmpdir.path, 'foo.'));

assert.match(path.basename(tmpFile.path), /^foo\.[0-9a-z]{6}$/i);
assert(fs.existsSync(tmpFile.path));

assert.strictEqual(typeof tmpFile.fd, 'number');
fs.appendFileSync(tmpFile.fd, 'bar');
fs.closeSync(tmpFile.fd);
assert.strictEqual(fs.readFileSync(tmpFile.path, 'utf-8'), 'bar');

const utf8 = fs.mkstempSync(path.join(tmpdir.path, '\u0222abc.'));
assert.strictEqual(Buffer.byteLength(path.basename(utf8.path)),
                   Buffer.byteLength('\u0222abc.XXXXXX'));
assert(fs.existsSync(utf8.path));
fs.closeSync(utf8.fd);

function handler(err, file) {
  assert.ifError(err);
  assert(fs.existsSync(file.path));
  assert.strictEqual(this, undefined);
  fs.closeSync(file.fd);
}

fs.mkstemp(path.join(tmpdir.path, 'bar.'), common.mustCall(handler));

// Same test as above, but making sure that passing an options object doesn't
// affect the way the callback function is handled.
fs.mkstemp(path.join(tmpdir.path, 'bar.'), {}, common.mustCall(handler));

const warningMsg = 'mkdtemp() and mkstemp() templates ending with X are ' +
                   'not portable. ' +
                   'For details see: https://nodejs.org/api/fs.html';
common.expectWarning('Warning', warningMsg);
fs.mkstemp(path.join(tmpdir.path, 'bar.X'), common.mustCall(handler));
