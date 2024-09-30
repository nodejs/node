'use strict';

const common = require('../common');
const assert = require('assert');
const { getPackageJSON } = require('module');


{ // Should throw when no arguments are provided
  assert.throws(
    () => getPackageJSON(),
    {
      code: 'ERR_INVALID_ARG_TYPE'
    }
  )
}
