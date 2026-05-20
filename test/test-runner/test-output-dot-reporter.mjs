// Test that the output of test-runner/output/dot_reporter.js matches test-runner/output/dot_reporter.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, specTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/dot_reporter.js'),
  specTransform,
);
