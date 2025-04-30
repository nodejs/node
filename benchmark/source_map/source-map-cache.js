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
    operation: [
      'findSourceMap-valid',
      'findSourceMap-invalid',
      'findSourceMap-missing',
      'findSourceMap-generated-source',
    ],
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

  const invalidFileName = path.resolve(
    __dirname,
    '../../test/fixtures/test-runner/source-maps/invalid-json/index.js',
  );
  const invalidMapFile = path.resolve(invalidFileName + '.map');
  const invalidFileContent = fs.readFileSync(invalidFileName, 'utf8');
  fs.readFileSync(invalidMapFile, 'utf8');

  const missingSourceURL = 'missing-source-url.js';

  const generatedSourceFileName = 'generated-source.js';
  const generatedSourceContent = eval(`
    function hello() {
      console.log('Hello, World!');
    }
    // # sourceMappingURL=${generatedSourceFileName}
  `);

  maybeCacheSourceMap(validFileName, validFileContent, null, false);
  maybeCacheSourceMap(invalidFileName, invalidFileContent, null, false);
  maybeCacheSourceMap(generatedSourceFileName, generatedSourceContent, null, true,
                      `/${generatedSourceFileName}`,
                      `${generatedSourceFileName}.map`);

  switch (operation) {
    case 'findSourceMap-valid':
      bench.start();
      for (let i = 0; i < n; i++) {
        findSourceMap(validFileName);
      }
      bench.end(n);
      break;

    case 'findSourceMap-invalid':
      bench.start();
      for (let i = 0; i < n; i++) {
        findSourceMap(invalidFileName);
      }
      bench.end(n);
      break;

    case 'findSourceMap-missing':
      bench.start();
      for (let i = 0; i < n; i++) {
        findSourceMap(missingSourceURL);
      }
      bench.end(n);
      break;

    case 'findSourceMap-generated-source':
      bench.start();
      for (let i = 0; i < n; i++) {
        findSourceMap(generatedSourceFileName);
      }
      bench.end(n);
      break;

    default:
      throw new Error(`Unknown operation: ${operation}`);
  }
}
