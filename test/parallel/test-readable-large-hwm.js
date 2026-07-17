'use strict';
const common = require('../common');
const { Readable } = require('stream');

// Make sure that readable completes
// even when reading larger buffer.
const bufferSize = 10 * 1024 * 1024;
let n = 0;
const r = new Readable({
  read() {
    // Try to fill readable buffer piece by piece.
    r.push(Buffer.alloc(bufferSize / 10));

    if (n++ > 10) {
      r.push(null);
    }
  }
});

r.on('readable', () => {
  while (true) {
    const ret = r.read(bufferSize);
    if (ret === null)
      break;
  }
});
r.on('end', common.mustCall());
