'use strict';
require('../common');
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const fs = require('fs');
const filepath = fixtures.path('x.txt');
const fd = fs.openSync(filepath, 'r');
const fsPromises = fs.promises;

const buffer = new Uint8Array();

assert.throws(
  () => fs.readSync(fd, buffer, 0, 10, 0),
  {
    code: 'ERR_INVALID_ARG_VALUE',
    message: 'The argument \'buffer\' is empty and cannot be written. ' +
    'Received Uint8Array []'
  }
);

assert.throws(
  () => fs.read(fd, buffer, 0, 1, 0, common.mustNotCall()),
  {
    code: 'ERR_INVALID_ARG_VALUE',
    message: 'The argument \'buffer\' is empty and cannot be written. ' +
    'Received Uint8Array []'
  }
);

(async () => {
  const filehandle = await fsPromises.open(filepath, 'r');
  assert.rejects(
    () => filehandle.read(buffer, 0, 1, 0),
    {
      code: 'ERR_INVALID_ARG_VALUE',
      message: 'The argument \'buffer\' is empty and cannot be written. ' +
               'Received Uint8Array []'
    }
  );
})();
