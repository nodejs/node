'use strict';

require('../common');
const assert = require('assert');

// One argument is required
assert.throws(() => {
  URL.canParse();
}, {
  code: 'ERR_MISSING_ARGS',
  name: 'TypeError',
});
