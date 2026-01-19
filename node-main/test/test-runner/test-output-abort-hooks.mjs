// Test that the output of test-runner/output/abort_hooks.js matches test-runner/output/abort_hooks.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/abort_hooks.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
