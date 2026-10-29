'use strict';

require('../common');
const { skipIfNoWatch } = require('../common/watch.js');

skipIfNoWatch();

const assert = require('assert');
const fs = require('fs');

assert.throws(
  () => fs.watch('.', { ignore: 123 }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  }
);

assert.throws(
  () => fs.watch('.', { ignore: '' }),
  {
    code: 'ERR_INVALID_ARG_VALUE',
    name: 'TypeError',
  }
);

assert.throws(
  () => fs.watch('.', { ignore: [123] }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  }
);

assert.throws(
  () => fs.watch('.', { ignore: [''] }),
  {
    code: 'ERR_INVALID_ARG_VALUE',
    name: 'TypeError',
  }
);
