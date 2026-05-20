import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

describe('vm output', { concurrency: !process.env.TEST_PARALLEL }, () => {
  const tests = [
    { name: 'vm/vm_caught_custom_runtime_error.js' },
    { name: 'vm/vm_display_runtime_error.js' },
    { name: 'vm/vm_display_syntax_error.js' },
    { name: 'vm/vm_dont_display_runtime_error.js' },
    { name: 'vm/vm_dont_display_syntax_error.js' },
  ];
  for (const { name } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), snapshot.defaultTransform);
    });
  }
});
