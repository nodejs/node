'use strict';

require('../common');
const assert = require('assert');

const { Readable, Writable, Transform } = require('stream');

{
  class RT extends Readable {
    constructor() {
      super({ objectMode: true });

      this.i = 0;
    }

    _read() {
      if (this.i === 30) {
        this.push(null);
      } else {
        this.push(this.i);
      }
      this.i++;
    }
  }

  const stream = new RT().acquireStandardStream();
  const reader = stream.getReader();
  const q = [];
  reader.read().then(function read({ value, done }) {
    if (done) {
      return;
    }
    q.push(value);
    return reader.read().then(read);
  }).then(() => {
    assert.deepStrictEqual(q, Array.from({ length: 30 }, (_, i) => i));
  });
}

{
  const q = [];

  class WT extends Writable {
    constructor() {
      super({ objectMode: true });
    }

    _write(chunk) {
      q.push(chunk);
    }

    _final(callback) {
      assert.deepStrictEqual(q, Array.from({ length: 30 }, (_, i) => i));
      callback();
    }
  }

  const stream = new WT().acquireStandardStream();
  const writer = stream.getWriter();
  let i = 0;
  writer.write(i).then(function cb() {
    if (i === 30) {
      return writer.close();
    }
    i++;
    return writer.write(i).then(cb);
  });
}

{
  class TT extends Transform {
    constructor() {
      super({ objectMode: true });
    }

    _transform(chunk, encoding, callback) {
      this.push(chunk * 2);
    }
  }

  const stream = new TT().acquireStandardStream();
  const writer = stream.writable.getWriter();
  let i = 0;
  writer.write(i).then(function cb() {
    if (i === 30) {
      return writer.close();
    }
    i++;
    return writer.write(i).then(cb);
  });

  const reader = stream.readable.getReader();
  const q = [];
  reader.read().then(function read({ value, done }) {
    if (done) {
      return;
    }
    q.push(value);
    return reader.read().then(read);
  }).then(() => {
    assert.deepStrictEqual(q, Array.from({ length: 30 }, (_, i) => i));
  });
}
