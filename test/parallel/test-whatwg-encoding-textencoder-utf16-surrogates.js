'use strict';

// From: https://github.com/w3c/web-platform-tests/blob/fa9436d12c/encoding/textencoder-utf16-surrogates.html

require('../common');

const assert = require('assert');
const {
  TextDecoder,
  TextEncoder
} = require('util');

const bad = [
  {
    input: '\uD800',
    expected: '\uFFFD',
    name: 'lone surrogate lead'
  },
  {
    input: '\uDC00',
    expected: '\uFFFD',
    name: 'lone surrogate trail'
  },
  {
    input: '\uD800\u0000',
    expected: '\uFFFD\u0000',
    name: 'unmatched surrogate lead'
  },
  {
    input: '\uDC00\u0000',
    expected: '\uFFFD\u0000',
    name: 'unmatched surrogate trail'
  },
  {
    input: '\uDC00\uD800',
    expected: '\uFFFD\uFFFD',
    name: 'swapped surrogate pair'
  },
  {
    input: '\uD834\uDD1E',
    expected: '\uD834\uDD1E',
    name: 'properly encoded MUSICAL SYMBOL G CLEF (U+1D11E)'
  }
];

bad.forEach((t) => {
  const encoded = new TextEncoder().encode(t.input);
  const decoded = new TextDecoder().decode(encoded);
  assert.strictEqual(decoded, t.expected);
});

assert.strictEqual(new TextEncoder().encode().length, 0);
