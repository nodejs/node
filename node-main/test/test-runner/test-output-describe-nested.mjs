// Test that the output of test-runner/output/describe_nested.js matches test-runner/output/describe_nested.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/describe_nested.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
