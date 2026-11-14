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

const setHref = Object.getOwnPropertyDescriptor(URL.prototype, 'href').set;
let calls = 0;

assert.throws(() => {
  setHref.call({}, {
    toString() {
      calls++;
      return 'https://example.com/';
    },
  });
}, {
  name: 'TypeError',
});

assert.strictEqual(calls, 0);
