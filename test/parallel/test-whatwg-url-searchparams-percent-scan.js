'use strict';

require('../common');
const assert = require('node:assert');

const cases = [
  ['%41', 'A'],
  ['%zz', '%zz'],
  ['%zz%41', '%zzA'],
  ['%', '%'],
  ['%A', '%A'],
  ['%C3%A9', '\u00e9'],
  ['%F0%9F%98%80', '\ud83d\ude00'],
  ['%FF', '\ufffd'],
  ['%E2%82', '\ufffd'],
];

for (const prefix of ['', 'k'.repeat(32), 'k'.repeat(1024), 'k+'.repeat(64)]) {
  const decodedPrefix = prefix.replaceAll('+', ' ');
  for (const [encoded, decoded] of cases) {
    for (const position of ['start', 'end']) {
      const input = position === 'start' ? encoded + prefix : prefix + encoded;
      const expected = position === 'start' ?
        decoded + decodedPrefix : decodedPrefix + decoded;
      const query = `${input}=${input}`;
      assert.deepStrictEqual([...new URLSearchParams(query)], [[expected, expected]]);
      const url = new URL(`https://example.org/?${query}`);
      assert.deepStrictEqual([...url.searchParams], [[expected, expected]]);
    }
  }
}
