'use strict';
const common = require('../common');
const assert = require('assert');

const { Readable, Transform } = require('stream');

let readCalls = 0;
class CustomReader extends Readable {
  constructor(opt) {
    super(opt);
    this._max = 1e7;
    this._index = 0;
  }

  _read(size) {
    readCalls++;

    while (this._index < this._max) {
      this._index++;

      if (this._index >= this._max) {
        return this.push(null);
      }

      if (!this.push('a')) {
        console.log('pause')
        return;
      } else {
        console.log('continue')
      }
    }
  }
}

new CustomReader()
  .on('end', common.mustCall())
  .pipe(new Transform({
    transform(chunk, encoding, cb) {
      process.nextTick(cb);
    }
  }))
  .on('finish', common.mustCall(() => {
    assert.strictEqual(readCalls, 610);
  }));
