// Test that the output of test-runner/output/global_after_should_fail_the_test.js matches
// test-runner/output/global_after_should_fail_the_test.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/global_after_should_fail_the_test.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
