// Flags: --experimental-modules
/* eslint-disable required-modules */

import assert from 'assert';
import fs, { readFile } from 'fs';
import main, { named } from
  '../fixtures/es-module-loaders/cjs-to-es-namespace.js';

assert(fs);
assert(fs.readFile);
assert.strictEqual(fs.readFile, readFile);

assert.strictEqual(main, 'default');
assert.strictEqual(named, 'named');
