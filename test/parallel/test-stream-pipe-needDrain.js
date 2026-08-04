'use strict';

const common = require('../common');
const assert = require('assert');
const { Readable, Writable } = require('stream');

// Pipe should pause temporarily if writable needs drain.
{
  const w = new Writable({
    write(buf, encoding, callback) {
      process.nextTick(callback);
    },
    highWaterMark: 1
  });

  while (w.write('asd'));

  assert.strictEqual(w.writableNeedDrain, true);

  const r = new Readable({
    read() {
      this.push('asd');
      this.push(null);
    }
  });

  r.on('pause', common.mustCall(2));
  r.on('end', common.mustCall());

  r.pipe(w);
}
