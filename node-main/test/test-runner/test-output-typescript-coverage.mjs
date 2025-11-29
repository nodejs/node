// Test that the output of test-runner/output/typescript-coverage.mts matches
// test-runner/output/typescript-coverage.snapshot
import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

if (!process.features.inspector) {
  common.skip('inspector support required');
}

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/typescript-coverage.mts'),
  defaultTransform,
  {
    flags: [
      '--disable-warning=ExperimentalWarning',
      '--test-reporter=tap',
      '--experimental-transform-types',
      '--experimental-test-module-mocks',
      '--experimental-test-coverage',
      '--test-coverage-exclude=!test/**',
    ],
  },
);
