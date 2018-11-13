'use strict';

// Tests below are not from WPT.
const common = require('../common');
const assert = require('assert');

const ref = new URL('http://example.com/path');
const url = new URL('http://example.com/path');
common.expectsError(() => {
  url.href = '';
}, {
  type: TypeError
});

assert.deepStrictEqual(url, ref);
