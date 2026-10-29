'use strict';

// Tests below are not from WPT.
require('../common');
const assert = require('assert');

const ref = new URL('http://example.com/path');
const url = new URL('http://example.com/path');
assert.throws(() => {
  url.href = '';
}, {
  name: 'TypeError'
});

assert.deepStrictEqual(url, ref);
