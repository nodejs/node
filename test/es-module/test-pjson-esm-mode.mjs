// Flags: --experimental-modules
/* eslint-disable node-core/required-modules */
import m from '../fixtures/es-modules/esm/module';
import assert from 'assert';

assert.strictEqual(m, 'esm');
