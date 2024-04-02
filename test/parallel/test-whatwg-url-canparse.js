// Flags: --expose-internals
'use strict';

require('../common');

const { URL } = require('url');
const assert = require('assert');

let internalBinding;
try {
  internalBinding = require('internal/test/binding').internalBinding;
} catch (e) {
  console.log('using `test/parallel/test-whatwg-url-canparse` requires `--expose-internals`');
  throw e;
}

const { canParse } = internalBinding('url');

// It should not throw when called without a base string
assert.strictEqual(URL.canParse('https://example.org'), true);
assert.strictEqual(canParse('https://example.org'), true);
