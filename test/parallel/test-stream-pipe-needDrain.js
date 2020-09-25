'use strict';

const common = require('../common');
const assert = require('assert');
const Readable = require('_stream_readable');
const Writable = require('_stream_writable');

// Pipe should not continue writing if writable needs drain.
{
  const w = new Writable({
    write(buf, encoding, callback) {

    }
  });

  while (w.write('asd'));

  assert.strictEqual(w.writableNeedDrain, true);

  const r = new Readable({
    read() {
      this.push('asd');
    }
  });

  w.write = common.mustNotCall();

  r.pipe(w);
}
