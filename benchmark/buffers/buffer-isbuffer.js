'use strict';
const common = require('../common');
const { Buffer } = require('buffer');

function FakeBuffer() {}
FakeBuffer.prototype = Object.create(Buffer.prototype);

// Refs: https://github.com/nodejs/node/issues/9531#issuecomment-265611061
class ExtensibleBuffer extends Buffer {
  constructor() {
    super(new ArrayBuffer(0), 0, 0);
    Object.setPrototypeOf(this, new.target.prototype);
  }
}

const inputs = {
  primitive: false,
  object: {},
  fake: new FakeBuffer(),
  subclassed: new ExtensibleBuffer(),
  fastbuffer: Buffer.alloc(1),
  slowbuffer: Buffer.allocUnsafeSlow(0),
  uint8array: new Uint8Array(1)
};

const bench = common.createBenchmark(main, {
  type: Object.keys(inputs),
  n: [1e7]
});

function main(conf) {
  const n = conf.n | 0;
  const input = inputs[conf.type];

  bench.start();
  for (var i = 0; i < n; i++)
    Buffer.isBuffer(input);
  bench.end(n);
}
