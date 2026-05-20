// Test that the output of test-runner/output/tap_escape.js matches test-runner/output/tap_escape.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import {
  spawnAndAssert,
  defaultTransform,
  ensureCwdIsProjectRoot,
} from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/tap_escape.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
