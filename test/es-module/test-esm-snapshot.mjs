// Flags: --experimental-modules
import '../common';
import '../fixtures/es-modules/esm-snapshot-mutator';
import one from '../fixtures/es-modules/esm-snapshot';
import assert from 'assert';

assert.strictEqual(one, 1);
