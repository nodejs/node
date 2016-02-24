'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const Buffer = require('buffer').Buffer;

common.refreshTmpDir();

const tmpFolder = fs.mkdtempSync(path.join(common.tmpDir, 'foo.'));

assert(path.basename(tmpFolder).length === 'foo.XXXXXX'.length);
assert(common.fileExists(tmpFolder));

const utf8 = fs.mkdtempSync(path.join(common.tmpDir, '\u0222abc.'));
assert.equal(Buffer.byteLength(path.basename(utf8)),
             Buffer.byteLength('\u0222abc.XXXXXX'));
assert(common.fileExists(utf8));

fs.mkdtemp(
    path.join(common.tmpDir, 'bar.'),
    common.mustCall(function(err, folder) {
      assert.ifError(err);
      assert(common.fileExists(folder));
    })
);
