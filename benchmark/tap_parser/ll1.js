'use strict';

const common = require('../common.js');
const path = require('path');
const { spawnSync } = require('child_process');

const pwd = path.resolve(__dirname, '../../');
const bench = common.createBenchmark(main, { n: [1024] });

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    spawnSync(process.execPath, ['--no-warnings', '--test',
                                 path.resolve(pwd, 'test/fixtures/test-runner/index.test.js'),
                                 path.resolve(pwd, 'test/fixtures/test-runner/nested.js'),
                                 path.resolve(pwd, 'test/fixtures/test-runner/invalid_tap.js'),
    ], { env: { REGEX_TAP_PARSER: 0 } });
  }
  bench.end(n);
}
