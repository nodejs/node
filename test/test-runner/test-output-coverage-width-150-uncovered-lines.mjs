// Test that the output of test-runner/output/coverage-width-150-uncovered-lines.mjs matches
// test-runner/output/coverage-width-150-uncovered-lines.snapshot
import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

if (!process.features.inspector) {
  common.skip('inspector support required');
}

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/coverage-width-150-uncovered-lines.mjs'),
  defaultTransform,
  { flags: ['--test-reporter=tap', '--test-coverage-exclude=!test/**'] },
);
