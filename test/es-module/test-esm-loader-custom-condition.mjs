// Flags: --experimental-loader ./test/fixtures/es-module-loaders/loader-with-custom-condition.mjs
import '../common/index.mjs';
import assert from 'assert';
import util from 'util';

import * as ns from '../fixtures/es-modules/conditional-exports.mjs';

assert.deepStrictEqual({ ...ns }, { default: 'from custom condition' });

assert.strictEqual(
  util.inspect(ns, { showHidden: false }),
  "[Module: null prototype] { default: 'from custom condition' }"
);

assert.strictEqual(
  util.inspect(ns, { showHidden: true }),
  '[Module: null prototype] {\n' +
  "  default: 'from custom condition',\n" +
  "  [Symbol(Symbol.toStringTag)]: 'Module'\n" +
  '}'
);
