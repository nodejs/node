import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

function replaceStackTrace(str) {
  return snapshot.replaceStackTrace(str, '$1at *$7\n');
}

describe('map output', { concurrency: true }, () => {
  function normalize(str) {
    return str
      .replaceAll(snapshot.replaceWindowsPaths(process.cwd()), '')
      .replaceAll('/', '*')
      .replaceAll(process.version, '*')
      .replaceAll('*test*fixtures*tick*', '*')
      .replaceAll(/:\d+/g, '*');
  }

  const common = snapshot
    .transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths);
  const defaultTransform = snapshot.transform(common);
  const customTransform = snapshot.transform(common, replaceStackTrace, normalize);

  const tests = [
    { name: 'tick/max_tick_depth.js' },
    { name: 'tick/nexttick_throw.js', transform: customTransform },
  ];

  for (const { name, transform } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform);
    });
  }
});
