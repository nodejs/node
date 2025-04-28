'use strict';

const common = require('../common.js');

const options = {
  flags: ['--expose-internals'],
};

const bench = common.createBenchmark(
  main,
  {
    operation: ['parse', 'findEntry', 'findOrigin'],
    n: [1e5],
  },
  options
);

function main({ operation, n }) {
  const { SourceMap } = require('internal/source_map/source_map');

  const samplePayload = {
    version: 3,
    file: 'out.js',
    sourceRoot: '',
    sources: ['foo.js', 'bar.js'],
    sourcesContent: ['console.log("foo");', 'console.log("bar");'],
    names: ['src', 'maps', 'are', 'fun'],
    mappings: 'A,AAAB;;ABCDE;',
  };
  const sourceMap = new SourceMap(samplePayload);

  switch (operation) {
    case 'parse':
      bench.start();
      for (let i = 0; i < n; i++) {
        new SourceMap(samplePayload);
      }
      bench.end(n);
      break;

    case 'findEntry':
      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMap.findEntry(1, 1);
      }
      bench.end(n);
      break;

    case 'findOrigin':
      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMap.findOrigin(2, 5);
      }
      bench.end(n);
      break;

    default:
      throw new Error(`Unknown operation: ${operation}`);
  }
}
