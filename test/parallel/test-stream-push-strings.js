'use strict';
require('../common');
const assert = require('assert');

const Readable = require('stream').Readable;

class MyStream extends Readable {
  constructor(options) {
    super(options);
    this._chunks = 3;
  }

  _read(n) {
    switch (this._chunks--) {
      case 0:
        return this.push(null);
      case 1:
        return setTimeout(() => {
          this.push('last chunk');
        }, 100);
      case 2:
        return this.push('second to last chunk');
      case 3:
        return process.nextTick(() => {
          this.push('first chunk');
        });
      default:
        throw new Error('?');
    }
  }
}

const ms = new MyStream();
const results = [];
ms.on('readable', function() {
  let chunk;
  while (null !== (chunk = ms.read()))
    results.push(String(chunk));
});

const expect = [ 'first chunksecond to last chunk', 'last chunk' ];
process.on('exit', function() {
  assert.strictEqual(ms._chunks, -1);
  assert.deepStrictEqual(results, expect);
  console.log('ok');
});
