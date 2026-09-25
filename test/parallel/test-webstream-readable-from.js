'use strict';

require('../common');
const assert = require('node:assert');

assert.throws(
  () => ReadableStream.from({}),
  { code: 'ERR_ARG_NOT_ITERABLE', name: 'TypeError' },
);

const invalidIterators = [
  { [Symbol.iterator]: () => 42 },
  { [Symbol.asyncIterator]: () => 42 },
];

for (const iterable of invalidIterators) {
  assert.throws(
    () => ReadableStream.from(iterable),
    { code: 'ERR_INVALID_STATE', name: 'TypeError' },
  );
}

function functionIterator() {}

// doesNotThrow
ReadableStream.from({
  [Symbol.iterator]: () => functionIterator,
});
