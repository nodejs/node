// Test that the output of test-runner/output/test-timeout-flag.js matches
// test-runner/output/test-timeout-flag.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/test-timeout-flag.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap', '--test-timeout=100'] },
);
