'use strict';

require('../common');
const assert = require('assert');

// Test invalid `paths` entries: Ensure non-string entries throw an error
{
  const paths = [1, false, null, undefined, () => {}, {}];
  paths.forEach((value) => {
    assert.throws(
      () => require.resolve('.', { paths: [value] }),
      {
        name: 'TypeError',
        code: 'ERR_INVALID_ARG_TYPE',
      }
    );
  });
}
