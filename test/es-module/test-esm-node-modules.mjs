// Flags: --experimental-modules
import '../common';
import assert from 'assert';
import { x } from '../fixtures/es-modules/node_lookups/module.mjs';

assert.deepStrictEqual(x, 42);
