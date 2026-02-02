import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

describe('v8 output', { concurrency: !process.env.TEST_PARALLEL }, () => {
  function normalize(str) {
    return str.replaceAll(/:\d+/g, ':*');
  }
  const defaultTransform = snapshot.transform(snapshot.basicTransform, snapshot.transformProjectRoot(), normalize);
  const tests = [
    { name: 'v8/v8_warning.js' },
  ];
  for (const { name } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), defaultTransform);
    });
  }
});
