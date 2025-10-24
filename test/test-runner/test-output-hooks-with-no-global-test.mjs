// Test that the output of test-runner/output/hooks-with-no-global-test.js matches
// test-runner/output/hooks-with-no-global-test.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/hooks-with-no-global-test.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
