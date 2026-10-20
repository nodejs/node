// Test that the output of test-runner/output/dot_reporter_coverage_threshold.js matches
// test-runner/output/dot_reporter_coverage_threshold.snapshot
import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, specTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

if (!process.features.inspector) {
  common.skip('inspector support required');
}

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/dot_reporter_coverage_threshold.js'),
  specTransform,
);
