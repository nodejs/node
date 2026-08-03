// Test that the output of test-runner/output/no_refs.js matches test-runner/output/no_refs.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/no_refs.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
