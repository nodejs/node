// Test that the output of test-runner/output/unfinished-suite-async-error.js matches
// test-runner/output/unfinished-suite-async-error.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/unfinished-suite-async-error.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
