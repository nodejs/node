'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const fs = require('fs');
const filepath = fixtures.path('testcodes.txt');
const fd = fs.openSync(filepath, 'r');

const buffer = new Uint8Array();

assert.throws(
  () => fs.readSync(fd, buffer, 0, 10, 0),
  {
    code: 'ERR_OUT_OF_RANGE',
    message: 'The value of "bufferLength" is out of range. ' +
            'It must be > 0. Received 0'
  }
);
