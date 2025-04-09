import { mustCall } from '../common/index.mjs';
import fixtures from '../common/fixtures.js';
import assert from 'assert';

assert.strictEqual(import.meta.main, true);

import(fixtures.path('/es-modules/import-meta-main.mjs')).then(
  mustCall(({ isMain }) => {
    assert.strictEqual(isMain, false);
  })
);
