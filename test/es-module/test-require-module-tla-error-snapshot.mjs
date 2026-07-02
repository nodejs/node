// Snapshot tests for the output of require(esm) on a graph with top-level await when
// --experimental-print-required-tla is enabled.

import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

const flags = ['--experimental-print-required-tla'];

describe('require(esm) with top-level await and --experimental-print-required-tla', () => {
  const tests = [
    // require()'d directly from a CommonJS entry point.
    { name: 'es-modules/tla/require-execution.js' },
    // require()'d through a chain of CommonJS files (multi-entry require stack),
    // with the top-level await in a transitive ES module dependency.
    { name: 'es-modules/tla/require-nested.js' },
    // Indented top-level await and for-await-of, to check caret alignment.
    { name: 'es-modules/tla/require-indented.js' },
    // `await using` declaration, which is top-level await without an
    // AwaitExpression or for-await-of node.
    { name: 'es-modules/tla/require-awaitusing.js' },
    // Preloaded via -r, so the require stack comes from internal/preload.
    {
      name: 'es-modules/tla/preload-main.js',
      flags: ['-r', fixtures.path('es-modules/tla/execution.mjs')],
    },
  ];
  for (const { name, flags: extraFlags = [] } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(
        fixtures.path(name),
        snapshot.defaultTransform,
        { flags: [...flags, ...extraFlags] },
      );
    });
  }
});
