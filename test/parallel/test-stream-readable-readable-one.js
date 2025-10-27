'use strict';
require('../common');
const assert = require('assert');

const { Readable } = require('stream');

// Read one buffer at a time and don't waste cycles allocating
// and copying into a new larger buffer.
{
  const r = new Readable({
    read() {}
  });
  const buffers = [Buffer.allocUnsafe(5), Buffer.allocUnsafe(10)];
  for (const buf of buffers) {
    r.push(buf);
  }
  for (const buf of buffers) {
    assert.strictEqual(r.read(), buf);
  }
}
