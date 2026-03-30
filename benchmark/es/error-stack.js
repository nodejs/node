'use strict';

const common = require('../common.js');
const modPath = require.resolve('../fixtures/simple-error-stack.js');
const nodeModulePath = require.resolve('../fixtures/node_modules/error-stack/simple-error-stack.js');
const Module = require('node:module');

const bench = common.createBenchmark(main, {
  method: [
    'without-sourcemap',
    'sourcemap',
    'node-modules-without-sourcemap',
    'node-modules-sourcemap'],
  n: [1e5],
});

function run(n, modPath) {
  delete require.cache[modPath];
  const mod = require(modPath);
  bench.start();
  for (let i = 0; i < n; i++) {
    mod.simpleErrorStack();
  }
  bench.end(n);
}

function main({ n, method }) {
  switch (method) {
    case 'without-sourcemap':
      process.setSourceMapsEnabled(false);
      run(n, modPath);
      break;
    case 'sourcemap':
      process.setSourceMapsEnabled(true);
      run(n, modPath);
      break;
    case 'node-modules-without-sourcemap':
      Module.setSourceMapsSupport(true, {
        nodeModules: false,
      });
      run(n, nodeModulePath);
      break;
    case 'node-modules-sourcemap':
      Module.setSourceMapsSupport(true, {
        nodeModules: true,
      });
      run(n, nodeModulePath);
      break;
    default:
      throw new Error(`Unexpected method "${method}"`);
  }
}
