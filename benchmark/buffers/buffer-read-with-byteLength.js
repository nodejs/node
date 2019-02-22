'use strict';
const common = require('../common.js');

const types = [
  'IntBE',
  'IntLE',
  'UIntBE',
  'UIntLE',
];

const bench = common.createBenchmark(main, {
  buffer: ['fast', 'slow'],
  type: types,
  n: [1e6],
  byteLength: [1, 2, 3, 4, 5, 6]
});

function main({ n, buf, type, byteLength }) {
  const clazz = buf === 'fast' ? Buffer : require('buffer').SlowBuffer;
  const buff = new clazz(8);
  const fn = `read${type || 'IntBE'}`;

  buff.writeDoubleLE(0, 0);
  bench.start();
  for (var i = 0; i !== n; i++) {
    buff[fn](0, byteLength);
  }
  bench.end(n);
}
