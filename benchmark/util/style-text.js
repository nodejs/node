'use strict';

const common = require('../common.js');

const { styleText } = require('node:util');
const assert = require('node:assert');

const bench = common.createBenchmark(main, {
  withColor: {
    messageType: ['string', 'number', 'boolean', 'invalid'],
    format: ['red', 'italic', 'invalid'],
    validateStream: [1, 0],
    noColor: [''],
    n: [1e3],
  },
  withoutColor: {
    messageType: ['string', 'number', 'boolean', 'invalid'],
    format: ['red', 'italic', 'invalid'],
    validateStream: [1, 0],
    noColor: ['1'],
    n: [1e3],
  },
}, { byGroups: true });

function main({ messageType, format, validateStream, noColor, n }) {
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

  process.env.NO_COLOR = noColor;

  bench.start();
  for (let i = 0; i < n; i++) {
    let colored = '';
    try {
      colored = styleText(format, str, { validateStream });
      assert.ok(colored); // Attempt to avoid dead-code elimination
    } catch {
      // eslint-disable no-empty
    }
  }
  bench.end(n);
}
