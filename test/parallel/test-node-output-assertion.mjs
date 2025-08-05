import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

function replaceStackTrace(str) {
  return snapshot.replaceStackTrace(str, '$1at *$7\n');
}

describe('assertion output', { concurrency: true }, () => {
  function normalize(str) {
    return str
      .replaceAll(snapshot.replaceWindowsPaths(process.cwd()), '')
      .replaceAll('/', '*')
      .replaceAll(process.version, '*')
      .replaceAll(/:\d+/g, ':*');
  }

  const common = snapshot
    .transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths);
  const defaultTransform = snapshot.transform(common, replaceStackTrace, normalize);

  const tests = [
    { name: 'assertion/assert_throws_stack.js' },
    { name: 'assertion/internal_assert_fail.js' },
    { name: 'assertion/internal_assert.js' },
  ];

  for (const { name, transform } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform);
    });
  }
});
