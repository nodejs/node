import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

describe('eval output', { concurrency: true }, () => {
  function normalize(str) {
    return str.replaceAll(snapshot.replaceWindowsPaths(process.cwd()), '')
      .replaceAll(/\d+:\d+/g, '*:*');
  }

  const defaultTransform = snapshot.transform(
    removeStackTraces,
    normalize,
    snapshot.replaceWindowsLineEndings,
    snapshot.replaceWindowsPaths,
    snapshot.replaceNodeVersion
  );

  function removeStackTraces(output) {
    return output.replaceAll(/^ *at .+$/gm, '');
  }

  const tests = [
    { name: 'eval/eval_messages.js' },
    { name: 'eval/stdin_messages.js' },
  ];

  for (const { name } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), defaultTransform);
    });
  }
});
