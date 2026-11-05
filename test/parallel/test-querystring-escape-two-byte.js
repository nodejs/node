'use strict';

require('../common');
const assert = require('node:assert');
const querystring = require('node:querystring');

for (let codePoint = 0x80; codePoint < 0x800; codePoint++) {
  const character = String.fromCharCode(codePoint);
  const input = `prefix${character}${character}/suffix`;
  const expected = encodeURIComponent(input);
  assert.strictEqual(querystring.escape(input), expected);
  assert.strictEqual(new URLSearchParams([[input, input]]).toString(), `${expected}=${expected}`);
}
