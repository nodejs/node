import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import path from 'node:path';

// should not reject
await import(path.resolve(fixtures.path('/es-module-node_ceiling/nested-without-node_ceiling/find-dep.mjs')));
await import(path.resolve(fixtures.path('/es-module-node_ceiling/find-dep.mjs')));

await assert.rejects(
  import(path.resolve(fixtures.path('/es-module-node_ceiling/nested-with-node_ceiling/dep-not-found.mjs'))),
  { code: 'ERR_MODULE_NOT_FOUND' },
);
