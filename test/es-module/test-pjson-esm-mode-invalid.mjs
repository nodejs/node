// Flags: --experimental-modules
/* eslint-disable required-modules */
import m from '../fixtures/es-modules/esm-invalid/module';
import assert from 'assert';

assert.strictEqual(m, 'cjs');
