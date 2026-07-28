import '../common/index.mjs';

import assert from 'assert';

import { createRequire } from 'module';

import mod from '../fixtures/es-modules/json-cache/mod.cjs';
import another from '../fixtures/es-modules/json-cache/another.cjs';
import test from '../fixtures/es-modules/json-cache/test.json' with
  { type: 'json' };

const require = createRequire(import.meta.url);

const modCjs = require('../fixtures/es-modules/json-cache/mod.cjs');
const anotherCjs = require('../fixtures/es-modules/json-cache/another.cjs');
const testCjs = require('../fixtures/es-modules/json-cache/test.json');

assert.strictEqual(mod.one, 1);
assert.strictEqual(another.one, 'zalgo');
assert.strictEqual(test.one, 'it comes');

assert.deepStrictEqual(mod, modCjs);
assert.deepStrictEqual(another, anotherCjs);
assert.deepStrictEqual(test, testCjs);
