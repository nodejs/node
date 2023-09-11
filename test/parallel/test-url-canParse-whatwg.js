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

{
  // This test is to ensure that the v8 fast api works.
  for (let i = 0; i < 1e5; i++) {
    assert(URL.canParse('https://www.example.com/path/?query=param#hash'));
  }
}
