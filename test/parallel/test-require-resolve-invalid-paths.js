'use strict';

require('../common');
const assert = require('assert');

// Test invalid `paths` entries: Ensure non-string entries throw an error
{
  const paths = [1, false, null, undefined, () => { }, {}];
  for (const path of paths) {
    assert.throws(
      () => require.resolve('.', { paths: [path] }),
      {
        name: 'TypeError',
        code: 'ERR_INVALID_ARG_TYPE',
      }
    );
  }
}
