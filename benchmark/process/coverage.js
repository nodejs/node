// This benchmark is meant to exercise a grab bag of code paths that would
// be expected to run slower under coverage.
'use strict';

const common = require('../common.js');
const bench = common.createBenchmark(main, {
  n: [1e5],
});
const { rmSync } = require('fs');
const { spawnSync } = require('child_process');
const tmpdir = require('../../test/common/tmpdir');

const coverageDir = tmpdir.resolve(`cov-${Date.now()}`);

function main({ n }) {
  bench.start();
  const result = spawnSync(process.execPath, [
    require.resolve('../fixtures/coverage-many-branches'),
  ], {
    env: {
      NODE_V8_COVERAGE: coverageDir,
      N: n,
      ...process.env,
    },
  });
  bench.end(n);
  rmSync(coverageDir, { recursive: true, force: true });
  if (result.status !== 0) {
    throw new Error(result.stderr.toString('utf8'));
  }
}
