// Test that the output of test-runner/output/output.js matches test-runner/output/output.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/output.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
