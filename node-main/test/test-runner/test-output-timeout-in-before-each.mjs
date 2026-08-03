// Test that the output of test-runner/output/timeout_in_before_each_should_not_affect_further_tests.js matches
// test-runner/output/timeout_in_before_each_should_not_affect_further_tests.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/timeout_in_before_each_should_not_affect_further_tests.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
