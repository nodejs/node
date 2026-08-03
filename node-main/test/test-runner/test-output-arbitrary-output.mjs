// Test that the output of test-runner/output/arbitrary-output.js matches
// test-runner/output/arbitrary-output.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/arbitrary-output.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
