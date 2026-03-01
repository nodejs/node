// Test that mocked CJS modules imported into ESM are excluded from coverage.
// This tests the fix for https://github.com/nodejs/node/issues/61709
import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

if (!process.features.inspector) {
  common.skip('inspector support required');
}

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/coverage-with-mock-cjs.mjs'),
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
