// Test that the output of test-runner/output/test-runner-plan-timeout.js matches
// test-runner/output/test-runner-plan-timeout.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/test-runner-plan-timeout.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap', '--test-force-exit'] },
);
