'use strict';

const common = require('../common.js');

const { styleText } = require('node:util');
const assert = require('node:assert');

// 1000 distinct hex colors to exercise the cache under high-miss conditions.
// Spread evenly across hue space so colors are valid and maximally varied.
const kHexColorCount = 1000;
const toHex = (n) => n.toString(16).padStart(2, '0');
const hexColors = Array.from({ length: kHexColorCount }, (_, i) => {
  const r = (i * 37) & 0xff;
  const g = (i * 73) & 0xff;
  const b = (i * 137) & 0xff;
  return `#${toHex(r)}${toHex(g)}${toHex(b)}`;
});

const bench = common.createBenchmark(main, {
  messageType: ['string', 'number', 'boolean', 'invalid'],
  // '#rotating' cycles through kHexColorCount distinct colors to simulate
  // the high-miss-rate / large-cache scenario (e.g. user-randomised colors).
  format: ['red', 'italic', 'invalid', '#ff0000', '#rotating'],
  validateStream: [1, 0],
  n: [1e3],
});

function main({ messageType, format, validateStream, n }) {
  let str;
  switch (messageType) {
    case 'string':
      str = 'hello world';
      break;
    case 'number':
      str = 10;
      break;
    case 'boolean':
      str = true;
      break;
    case 'invalid':
      str = undefined;
      break;
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    const fmt = format === '#rotating' ? hexColors[i % kHexColorCount] : format;
    let colored = '';
    try {
      colored = styleText(fmt, str, { validateStream });
      assert.ok(colored); // Attempt to avoid dead-code elimination
    } catch {
      // eslint-disable no-empty
    }
  }
  bench.end(n);
}
