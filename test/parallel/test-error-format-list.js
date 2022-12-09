// Flags: --expose-internals
'use strict';

require('../common');
const { strictEqual } = require('node:assert');
const { formatList } = require('internal/errors');

strictEqual(formatList([]), '');

strictEqual(formatList([], 'or'), '');

strictEqual(formatList(['apple']), 'apple');

strictEqual(formatList(['apple'], 'or'), 'apple');

strictEqual(formatList(['apple', 'banana']), 'apple and banana');

strictEqual(formatList(['apple', 'banana'], 'or'), 'apple or banana');

strictEqual(
  formatList(['apple', 'banana', 'orange']),
  'apple, banana, and orange'
);

strictEqual(
  formatList(['apple', 'banana', 'orange'], 'or'),
  'apple, banana, or orange'
);
