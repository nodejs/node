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
    findSourceMap,
  } = require('internal/source_map/source_map_cache');

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
      bench.start();
      maybeCacheSourceMap(validFileName, validFileContent, fakeModule, false);

      for (let i = 0; i < n; i++) {
        sourceMap = findSourceMap(validFileName);
      }
      bench.end(n);
      break;

    case 'findSourceMap-generated-source':
      bench.start();
      maybeCacheSourceMap(
        validFileName,
        validFileContent,
        fakeModule,
        true,
        validFileName,
        validMapFile,
      );

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
