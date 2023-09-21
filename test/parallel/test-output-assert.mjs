import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

function replaceStackTrace(str) {
  return snapshot.replaceStackTrace(str, '$1at *$7\n');
}
function normalize(str) {
  return str
    .replaceAll(snapshot.replaceWindowsPaths(process.cwd()), '')
    .replaceAll('/', '*')
    .replaceAll(process.version, '*')
    .replaceAll(/:\d+/g, '*');
}
const common = snapshot
  .transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths);

const defaultTransform = snapshot.transform(common, replaceStackTrace, normalize);

const tests = [
  { name: 'assert/assert_throws_stack.js' },
  { name: 'assert/internal_assert_fail.js' },
  { name: 'assert/internal_assert.js' },
];

describe('assertion output', { concurrency: true }, () => {
  for (const { name, transform } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform);
    });
  }
});
