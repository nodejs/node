// Test that the output of test-runner/output/test-runner-plan.js matches
// test-runner/output/test-runner-plan.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/test-runner-plan.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
