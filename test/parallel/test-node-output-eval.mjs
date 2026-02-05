import * as common from '../common/index.mjs';
if (!process.config.variables.node_use_amaro) {
  common.skip('Requires Amaro');
}
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

describe('eval output', { concurrency: true }, () => {
  function normalize(str) {
    return str
      .replaceAll(/\d+:\d+/g, '*:*');
  }

  const defaultTransform = snapshot.transform(
    normalize,
    snapshot.basicTransform,
    snapshot.transformProjectRoot(),
    removeStackTraces,
  );

  function removeStackTraces(output) {
    return output.replaceAll(/^ *at .+$/gm, '');
  }

  const tests = [
    { name: 'eval/eval_messages.js' },
    { name: 'eval/stdin_messages.js' },
    { name: 'eval/stdin_typescript.js' },
    { name: 'eval/eval_typescript.js' },
  ];

  for (const { name } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), defaultTransform);
    });
  }
});
