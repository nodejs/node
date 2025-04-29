'use strict';

const common = require('../common.js');
const fs = require('fs');
const path = require('path');

const options = {
  flags: ['--expose-internals'],
};

const bench = common.createBenchmark(
  main,
  {
    operation: ['findSourceMap-valid'],
    n: [1e5],
  },
  options,
);

function main({ operation, n }) {
  const {
    findSourceMap,
    maybeCacheSourceMap,
  } = require('internal/source_map/source_map_cache');

  const validFileName = path.resolve(
    __dirname,
    '../../test/fixtures/test-runner/source-maps/line-lengths/index.js',
  );
  const validMapFile = path.resolve(validFileName + '.map');
  const validFileContent = fs.readFileSync(validFileName, 'utf8');
  fs.readFileSync(validMapFile, 'utf8');

  maybeCacheSourceMap(validFileName, validFileContent, null, false);

  switch (operation) {
    case 'findSourceMap-valid':
      bench.start();
      for (let i = 0; i < n; i++) {
        findSourceMap(validFileName);
      }
      bench.end(n);
      break;

    default:
      throw new Error(`Unknown operation: ${operation}`);
  }
}
