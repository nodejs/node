'use strict';
const common = require('../common');
const fs = require('fs');

if (!common.isMacOS) {
  common.skip('this tests works only on MacOS');
}

fs.readdir(
  Buffer.from('/dev'),
  { withFileTypes: true, encoding: 'buffer' },
  common.mustSucceed()
);
