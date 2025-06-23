'use strict';

const common = require('../common.js');
const assert = require('assert');
const fixtures = require('../../test/common/fixtures');

const bench = common.createBenchmark(
  main,
  {
    operation: [
      'findSourceMap-valid',
      'findSourceMap-generated-source',
    ],
    n: [1e5],
  },
);

function main({ operation, n }) {
  const Module = require('node:module');

  Module.setSourceMapsSupport(true, {
    generatedCode: true,
  });
  const validFileName = fixtures.path('test-runner/source-maps/line-lengths/index.js')

  const fileNameKey = '/source-map/disk.js';
  const generatedFileName = fixtures.path(fileNameKey);
  const generatedFileContent = fixtures.readSync(fileNameKey, 'utf8');
  const sourceMapUrl = generatedFileName.replace(/\.js$/, '.map');
  const sourceWithGeneratedSourceMap =
    `${generatedFileContent}\n//# sourceMappingURL=${sourceMapUrl}\n//# sourceURL=${generatedFileName}`;
  const generatedExpectedUrl = `file://${generatedFileName}`;

  let sourceMap;
  switch (operation) {
    case 'findSourceMap-valid':
      require(validFileName);

      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMap = Module.findSourceMap(validFileName);
      }
      bench.end(n);
      break;

    case 'findSourceMap-generated-source':
      eval(sourceWithGeneratedSourceMap);

      bench.start();
      for (let i = 0; i < n; i++) {
        sourceMap = Module.findSourceMap(generatedExpectedUrl);
      }
      bench.end(n);
      break;

    default:
      throw new Error(`Unknown operation: ${operation}`);
  }
  assert.ok(sourceMap);
}
