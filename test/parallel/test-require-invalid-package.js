'use strict';

require('../common');
const assert = require('assert');

// Should be an invalid package path.
assert.throws(() => require('package.json'), (err) => {
  return err && err.code === 'MODULE_NOT_FOUND';
});
