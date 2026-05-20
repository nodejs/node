import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

const skipForceColors =
  process.config.variables.icu_gyp_path !== 'tools/icu/icu-generic.gyp' ||
  process.config.variables.node_shared_openssl;

describe('console output', { concurrency: !process.env.TEST_PARALLEL }, () => {
  const tests = [
    { name: 'console/2100bytes.js' },
    { name: 'console/console_low_stack_space.js' },
    { name: 'console/console.js' },
    { name: 'console/hello_world.js' },
    { name: 'console/stack_overflow.js' },
    !skipForceColors ? { name: 'console/force_colors.js', env: { FORCE_COLOR: 1 } } : null,
  ].filter(Boolean);
  for (const { name, env } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(
        fixtures.path(name),
        snapshot.defaultTransform,
        { env: { ...env, ...process.env } },
      );
    });
  }
});
