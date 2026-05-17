'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  input: [
    'arraybuffer',
    'buffer',
    'dataview',
    'uint8array',
    'uint8clampedarray',
    'int8array',
    'uint16array',
    'int16array',
    'uint32array',
    'int32array',
    'float16array',
    'float32array',
    'float64array',
    'bigint64array',
    'biguint64array',
  ],
  n: [1e6],
}, { flags: ['--expose-internals'] });

function main({ n, input }) {
  const { converters } = require('internal/webidl');

  let value;
  switch (input) {
    case 'arraybuffer': value = new ArrayBuffer(32); break;
    case 'buffer': value = Buffer.alloc(32); break;
    case 'dataview': value = new DataView(new ArrayBuffer(32)); break;
    case 'uint8array': value = new Uint8Array(32); break;
    case 'uint8clampedarray': value = new Uint8ClampedArray(32); break;
    case 'int8array': value = new Int8Array(32); break;
    case 'uint16array': value = new Uint16Array(16); break;
    case 'int16array': value = new Int16Array(16); break;
    case 'uint32array': value = new Uint32Array(8); break;
    case 'int32array': value = new Int32Array(8); break;
    case 'float16array': value = new Float16Array(16); break;
    case 'float32array': value = new Float32Array(8); break;
    case 'float64array': value = new Float64Array(4); break;
    case 'bigint64array': value = new BigInt64Array(4); break;
    case 'biguint64array': value = new BigUint64Array(4); break;
  }

  const opts = { prefix: 'test', context: 'test' };

  bench.start();
  for (let i = 0; i < n; i++)
    converters.BufferSource(value, opts);
  bench.end(n);
}
