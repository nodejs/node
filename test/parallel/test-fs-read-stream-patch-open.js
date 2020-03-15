'use strict';
const common = require('../common');
const fs = require('fs');

common.expectWarning(
  'DeprecationWarning',
  'ReadStream.prototype.open() is deprecated', 'DEP0135');
const s = fs.createReadStream('asd')
  // We don't care about errors in this test.
  .on('error', () => {});
s.open();

process.nextTick(() => {
  // Allow overriding open().
  fs.ReadStream.prototype.open = common.mustCall();
  fs.createReadStream('asd');
});
