// Test that the output of test-runner/output/arbitrary-output-colored.js matches
// test-runner/output/arbitrary-output-colored.snapshot
import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import {
  spawnAndAssert,
  specTransform,
  replaceTestDuration,
  transform,
  ensureCwdIsProjectRoot,
} from '../common/assertSnapshot.js';

const skipForceColors =
  process.config.variables.icu_gyp_path !== 'tools/icu/icu-generic.gyp' ||
  process.config.variables.node_shared_openssl;

if (skipForceColors) {
  // https://github.com/nodejs/node/pull/48057
  common.skip('Forced colors not supported in this build');
}

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/arbitrary-output-colored.js'),
  transform(specTransform, replaceTestDuration),
  { tty: true },
);
