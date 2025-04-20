import { mustCall } from '../common/index.mjs';
import assert from 'node:assert/strict';
import { Worker } from 'node:worker_threads';

assert.strictEqual(import.meta.main, true);

import('../fixtures/es-modules/import-meta-main.mjs').then(
  mustCall(({ isMain }) => {
    assert.strictEqual(isMain, false);
  })
);

if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  new Worker(import.meta.filename);
}
