'use strict';
const common = require('../common.js');
const { fileURLToPath, pathToFileURL } = require('node:url');
const isWindows = process.platform === 'win32';

const bench = common.createBenchmark(main, {
  input: isWindows ? [
    'file:///c/',
  ] : [
    'file:///dev/null',
    'file:///dev/null?key=param&bool',
    'file:///dev/null?key=param&bool#hash',
  ],
  method: isWindows ? [
    'fileURLToPath',
  ] : [
    'fileURLToPath',
    'pathToFileURL',
  ],
  n: [5e6],
});

function main({ n, input, method }) {
  method = method === 'fileURLOrPath' ? fileURLToPath : pathToFileURL;
  bench.start();
  for (let i = 0; i < n; i++) {
    method(input);
  }
  bench.end(n);
}
