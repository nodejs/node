'use strict';
require('../common');
const assert = require('assert');
const { ZipArchive } = require('zlib');
const fixtures = require('../common/fixtures');

const archive = fixtures.readSync('elipses.txt');

assert.throws(() => {
  new ZipArchive(archive);
}, {
  message: 'Zip opening failed: Not a zip archive',
  code: 'ERR_ZIP_OPENING_FAILED',
  name: 'Error',
});
