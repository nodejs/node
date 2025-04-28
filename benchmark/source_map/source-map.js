'use strict';

const common = require('../common.js');

const options = {
  flags: ['--expose-internals'],
};

const bench = common.createBenchmark(
  main,
  {
    operation: [
      'parse',
      'parse-minified',
      'parse-sectioned',
      'parse-large',
      'findEntry',
      'findOrigin',
    ],
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
    sourcesContent: [null, null],
    names: ['src', 'maps', 'are', 'fun'],
    mappings: 'A,AAAB;;ABCDE;',
  };
  const minifiedPayload = {
    version: 3,
    file: 'add.min.js',
    sourceRoot: '',
    sources: ['add.ts'],
    names: ['add', 'x', 'y'],
    mappings:
      'AAAA,QAASA,EAAGC,EAAGC,CAClB,OAAOD,EAAGC,GACZC,QAAQC,IAAI,CAAC,CAAC,CAAC',
  };
  const sectionedPayload = {
    version: 3,
    file: 'app.js',
    sections: [
      {
        offset: { line: 100, column: 10 },
        map: {
          version: 3,
          file: 'section.js',
          sources: ['foo.js', 'bar.js'],
          names: ['src', 'maps', 'are', 'fun'],
          mappings: 'AAAA,E;;ABCDE;',
        },
      },
    ],
  };
  const largePayload = {
    version: 3,
    file: 'out.js',
    sources: Array.from({ length: 1000 }, (_, i) => `source-${i}.js`),
    names: Array.from({ length: 1000 }, (_, i) => `name-${i}`),
    mappings: 'A,AAAB;;ABCDE;'.repeat(1000),
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

    case 'parse-minified':
      bench.start();
      for (let i = 0; i < n; i++) {
        new SourceMap(minifiedPayload);
      }
      bench.end(n);
      break;

    case 'parse-sectioned':
      bench.start();
      for (let i = 0; i < n; i++) {
        new SourceMap(sectionedPayload);
      }
      bench.end(n);
      break;

    case 'parse-large':
      bench.start();
      for (let i = 0; i < n; i++) {
        new SourceMap(largePayload);
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
