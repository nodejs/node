import '../common/index.mjs';
import '../fixtures/es-modules/esm-snapshot-mutator.js';
import one from '../fixtures/es-modules/esm-snapshot.js';
import assert from 'assert';

assert.strictEqual(one, 1);
