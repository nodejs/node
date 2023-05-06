import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

function replaceNodeVersion(str) {
  return str.replaceAll(process.version, '*');
}

function replaceStackTrace(str) {
  return snapshot.replaceStackTrace(str, '$1at *$7\n');
}

describe('assertion output', { concurrency: true }, () => {
  function normalize(str) {
    return str.replaceAll(snapshot.replaceWindowsPaths(process.cwd()), '').replaceAll('//', '*').replaceAll(/\/(\w)/g, '*$1').replaceAll('*test*', '*').replaceAll('*fixtures*assertion*', '*').replaceAll('file:**', 'file:*/');
  }

  function normalizeNoNumbers(str) {
    return normalize(str).replaceAll(/\d+:\d+/g, '*:*').replaceAll(/:\d+/g, ':*').replaceAll('*fixtures*message*', '*');
  }
  const common = snapshot
    .transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths, replaceNodeVersion);
  const defaultTransform = snapshot.transform(common, normalize);
  const customTransform = snapshot.transform(common, replaceStackTrace, normalizeNoNumbers);
  const tests = [
    { name: 'assertion/assert_throws_stack.js', transform: customTransform },
    { name: 'assertion/internal_assert_fail.js', transform: customTransform },
    { name: 'assertion/internal_assert.js', transform: customTransform },
  ];
  for (const { name, transform } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform);
    });
  }
});
