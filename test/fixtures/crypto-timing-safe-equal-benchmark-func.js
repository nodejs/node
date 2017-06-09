'use strict';

const assert = require('assert');
module.exports = (compareFunc, firstBufFill, secondBufFill, bufSize) => {
  const firstBuffer = Buffer.alloc(bufSize, firstBufFill);
  const secondBuffer = Buffer.alloc(bufSize, secondBufFill);

  const startTime = process.hrtime();
  const result = compareFunc(firstBuffer, secondBuffer);
  const endTime = process.hrtime(startTime);

  // Ensure that the result of the function call gets used, so it doesn't
  // get discarded due to engine optimizations.
  assert.strictEqual(result, firstBufFill === secondBufFill);

  return endTime[0] * 1e9 + endTime[1];
};
