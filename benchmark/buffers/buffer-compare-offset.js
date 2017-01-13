'use strict';
const common = require('../common.js');
const v8 = require('v8');

const bench = common.createBenchmark(main, {
  method: ['offset', 'slice'],
  size: [16, 512, 1024, 4096, 16386],
  millions: [1]
});

function compareUsingSlice(b0, b1, len, iter) {

  // Force optimization before starting the benchmark
  Buffer.compare(b0.slice(1, len), b1.slice(1, len));
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(Buffer.compare)');
  eval('%OptimizeFunctionOnNextCall(b0.slice)');
  eval('%OptimizeFunctionOnNextCall(b1.slice)');
  Buffer.compare(b0.slice(1, len), b1.slice(1, len));
  doCompareUsingSlice(b0, b1, len, iter);
}

function doCompareUsingSlice(b0, b1, len, iter) {
  var i;
  bench.start();
  for (i = 0; i < iter; i++)
    Buffer.compare(b0.slice(1, len), b1.slice(1, len));
  bench.end(iter / 1e6);
}

function compareUsingOffset(b0, b1, len, iter) {
  len = len + 1;
  // Force optimization before starting the benchmark
  b0.compare(b1, 1, len, 1, len);
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(b0.compare)');
  b0.compare(b1, 1, len, 1, len);
  doCompareUsingOffset(b0, b1, len, iter);
}

function doCompareUsingOffset(b0, b1, len, iter) {
  var i;
  bench.start();
  for (i = 0; i < iter; i++)
    b0.compare(b1, 1, len, 1, len);
  bench.end(iter / 1e6);
}

function main(conf) {
  const iter = (conf.millions >>> 0) * 1e6;
  const size = (conf.size >>> 0);
  const method = conf.method === 'slice' ?
      compareUsingSlice : compareUsingOffset;
  method(Buffer.alloc(size, 'a'),
         Buffer.alloc(size, 'b'),
         size >> 1,
         iter);
}
