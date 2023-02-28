'use strict';

const common = require('../common.js');
const ffi = require('node:ffi');

const bench = common.createBenchmark(main, {
  n: [1e7],
});

const getpid = ffi.getNativeFunction(null, 'uv_os_getpid', 'int', []);

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i)
    getpid();
  bench.end(n);

}
