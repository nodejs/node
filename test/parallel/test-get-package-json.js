'use strict';

const common = require('../common');
const assert = require('assert');
const { getPackageJSON } = require('module');


{ // Throws when no arguments are provided
  assert.throws(
    () => getPackageJSON(),
    { code: 'ERR_INVALID_ARG_TYPE' }
  );
}

{ // Throw when `everything` is not a boolean
  for (const invalid of ['', {}, [], Symbol(), 1, 0, ]) assert.throws(
    () => getPackageJSON('', invalid),
    { code: 'ERR_INVALID_ARG_TYPE' }
  );
}
