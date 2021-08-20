'use strict';
require('../common');

const assert = require('assert');
const fs = require('fs');

{
  const fd = 'k';

  assert.throws(
    () => {
      fs.createReadStream(null, { fd });
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });

  assert.throws(
    () => {
      fs.createWriteStream(null, { fd });
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });
}

{
  const path = 46;

  assert.throws(
    () => {
      fs.createReadStream(path);
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });

  assert.throws(
    () => {
      fs.createWriteStream(path);
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });
}
