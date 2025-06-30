'use strict';

const common = require('../common.js');
const assert = require('assert');
const fixtures = require('../../test/common/fixtures');

const bench = common.createBenchmark(
  main,
  {
    operation: [
      'parse',
      'parse-sectioned',
      'findEntry',
      'findEntry-sectioned',
      'findOrigin',
      'findOrigin-sectioned',
    ],
    n: [1e5],
  },
);

function main({ operation, n }) {
  const { SourceMap } = require('node:module');

  const samplePayload = JSON.parse(
    fixtures.readSync('source-map/no-source.js.map', 'utf8'),
  );
  const sectionedPayload = JSON.parse(
    fixtures.readSync('source-map/disk-index.map', 'utf8'),
  );

  let sourceMap;
  let sourceMapMethod;
  switch (operation) {
    case 'parse':
      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMap = new SourceMap(samplePayload);
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

    case 'findEntry':
      sourceMap = new SourceMap(samplePayload);
      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMapMethod = sourceMap.findEntry(i, i);
      }
      bench.end(n);
      assert.ok(sourceMapMethod);
      break;

    case 'findEntry-sectioned':
      sourceMap = new SourceMap(sectionedPayload);
      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMapMethod = sourceMap.findEntry(i, i);
      }
      bench.end(n);
      assert.ok(sourceMapMethod);
      break;

    case 'findOrigin':
      sourceMap = new SourceMap(samplePayload);
      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMapMethod = sourceMap.findOrigin(i, i);
      }
      bench.end(n);
      assert.ok(sourceMapMethod);
      break;

    case 'findOrigin-sectioned':
      sourceMap = new SourceMap(sectionedPayload);
      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMapMethod = sourceMap.findOrigin(i, i);
      }
      bench.end(n);
      assert.ok(sourceMapMethod);
      break;

    default:
      throw new Error(`Unknown operation: ${operation}`);
  }
  assert.ok(sourceMap);
}
