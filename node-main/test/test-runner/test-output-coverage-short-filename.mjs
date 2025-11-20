// Test that the output of test-runner/output/coverage-short-filename.mjs matches
// test-runner/output/coverage-short-filename.snapshot
import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

if (!process.features.inspector) {
  common.skip('inspector support required');
}

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/coverage-short-filename.mjs'),
  defaultTransform,
  {
    flags: ['--test-reporter=tap', '--test-coverage-exclude=../output/**'],
    cwd: fixtures.path('test-runner/coverage-snap'),
  },
);
