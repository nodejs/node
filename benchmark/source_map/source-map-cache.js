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
      'findSourceMap-valid',
      'findSourceMap-generated-source',
    ],
    n: [1e5],
  },
  options,
);

function main({ operation, n }) {
  const {
    setSourceMapsSupport,
    maybeCacheSourceMap,
  } = require('internal/source_map/source_map_cache');
  const { findSourceMap } = require('node:module');

  setSourceMapsSupport(true);
  const validFileName = path.resolve(
    __dirname,
    '../../test/fixtures/test-runner/source-maps/line-lengths/index.js',
  );
  const validMapFile = path.resolve(validFileName + '.map');
  const validFileContent = fs.readFileSync(validFileName, 'utf8');
  const fakeModule = { filename: validFileName };

  let sourceMap;
  switch (operation) {
    case 'findSourceMap-valid':
      maybeCacheSourceMap(validFileName, validFileContent, fakeModule, false, undefined, validMapFile);
      bench.start();

      for (let i = 0; i < n; i++) {
        sourceMap = findSourceMap(validFileName);
      }
      bench.end(n);
      break;

    case 'findSourceMap-generated-source':
      maybeCacheSourceMap(
        validFileName,
        validFileContent,
        fakeModule,
        true,
        validFileName,
        validMapFile,
      );
      bench.start();

      for (let i = 0; i < n; i++) {
        sourceMap = findSourceMap(validFileName);
      }
      bench.end(n);
      break;

    default:
      throw new Error(`Unknown operation: ${operation}`);
  }
  assert.ok(sourceMap);
}
