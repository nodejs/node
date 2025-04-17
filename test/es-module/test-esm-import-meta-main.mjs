import { mustCall } from '../common/index.mjs';
import assert from 'assert';

assert.strictEqual(import.meta.main, true);

import('../fixtures/es-modules/import-meta-main.mjs').then(
  mustCall(({ isMain }) => {
    assert.strictEqual(isMain, false);
  })
);
