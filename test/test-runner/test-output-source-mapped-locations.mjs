// Test that the output of test-runner/output/source_mapped_locations.mjs matches
// test-runner/output/source_mapped_locations.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/source_mapped_locations.mjs'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
