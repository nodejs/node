// Test that the output of test-runner/output/describe_it.js matches test-runner/output/describe_it.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/describe_it.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
