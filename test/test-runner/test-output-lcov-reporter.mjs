// Test that the output of test-runner/output/lcov_reporter.js matches test-runner/output/lcov_reporter.snapshot
import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, lcovTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

if (!process.features.inspector) {
  common.skip('inspector support required');
}

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/lcov_reporter.js'),
  lcovTransform,
);
