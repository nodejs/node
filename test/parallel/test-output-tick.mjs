import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

describe('map output', { concurrency: true }, () => {
  function normalize(str) {
    return str
      .replaceAll(snapshot.replaceWindowsPaths(process.cwd()), '')
      .replaceAll('/test/fixtures/tick/', '*test*message*')
      .replaceAll(process.version, '*')
      .replaceAll(/\d+/g, '*');
  }

  const common = snapshot.transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths);
  
  const defaultTransform = snapshot.transform(common);
  const noNextTickThrowTransform = snapshot.transform(common, normalize);

  const tests = [
    { name: 'tick/max_tick_depth.js' },
    { name: 'tick/nexttick_throw.js', transform: noNextTickThrowTransform },
  ];

  for (const { name, transform } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform);
    });
  }
});
