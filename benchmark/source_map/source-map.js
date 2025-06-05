'use strict';

const common = require('../common.js');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

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
      'findEntry-minified',
      'findEntry-sectioned',
      'findEntry-large',
      'findOrigin',
      'findOrigin-minified',
      'findOrigin-sectioned',
      'findOrigin-large',
    ],
    n: [1e5],
  },
  options,
);

function main({ operation, n }) {
  const { SourceMap } = require('internal/source_map/source_map');

  const samplePayload = JSON.parse(
    fs.readFileSync(
      path.resolve(__dirname, '../../test/fixtures/source-map/no-source.js.map'),
      'utf8',
    ),
  );
  const minifiedPayload = JSON.parse(
    fs.readFileSync(
      path.resolve(__dirname, '../../test/fixtures/source-map/enclosing-call-site.js.map'),
      'utf8',
    ),
  );
  const sectionedPayload = JSON.parse(
    fs.readFileSync(
      path.resolve(__dirname, '../../test/fixtures/source-map/disk-index.map'),
      'utf8',
    ),
  );
  const largePayload = JSON.parse(
    fs.readFileSync(
      path.resolve(__dirname, '../../test/fixtures/test-runner/source-maps/line-lengths/index.js.map'),
      'utf8',
    ),
  );

  let sourceMap;
  switch (operation) {
    case 'parse':
      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMap = new SourceMap(samplePayload);
      }
      bench.end(n);
      break;

    case 'parse-minified':
      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMap = new SourceMap(minifiedPayload);
      }
      bench.end(n);
      break;

    case 'parse-sectioned':
      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMap = new SourceMap(sectionedPayload);
      }
      bench.end(n);
      break;

    case 'parse-large':
      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMap = new SourceMap(largePayload);
      }
      bench.end(n);
      break;

    case 'findEntry':
      sourceMap = new SourceMap(samplePayload);
      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMap.findEntry(i, i);
      }
      bench.end(n);
      break;

    case 'findEntry-minified':
      sourceMap = new SourceMap(minifiedPayload);
      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMap.findEntry(i, i);
      }
      bench.end(n);
      break;

    case 'findEntry-sectioned':
      sourceMap = new SourceMap(sectionedPayload);
      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMap.findEntry(i, i);
      }
      bench.end(n);
      break;

    case 'findEntry-large':
      sourceMap = new SourceMap(largePayload);
      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMap.findEntry(i, i);
      }
      bench.end(n);
      break;

    case 'findOrigin':
      sourceMap = new SourceMap(samplePayload);
      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMap.findOrigin(i, i);
      }
      bench.end(n);
      break;

    case 'findOrigin-minified':
      sourceMap = new SourceMap(minifiedPayload);
      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMap.findOrigin(i, i);
      }
      bench.end(n);
      break;

    case 'findOrigin-sectioned':
      sourceMap = new SourceMap(sectionedPayload);
      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMap.findOrigin(i, i);
      }
      bench.end(n);
      break;

    case 'findOrigin-large':
      sourceMap = new SourceMap(largePayload);
      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMap.findOrigin(i, i);
      }
      bench.end(n);
      break;

    default:
      throw new Error(`Unknown operation: ${operation}`);
  }
  assert.ok(sourceMap);
}
