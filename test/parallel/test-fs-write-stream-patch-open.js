'use strict';
const common = require('../common');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

common.expectWarning(
  'DeprecationWarning',
  'WriteStream.prototype.open() is deprecated', 'DEP0XXX');
const s = fs.createWriteStream(`${tmpdir.path}/out`);
s.open();
s.destroy();

// Allow overriding open().
fs.WriteStream.prototype.open = common.mustCall();
fs.createWriteStream('asd');
