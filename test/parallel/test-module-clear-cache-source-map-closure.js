// Flags: --enable-source-maps
'use strict';

require('../common');

const assert = require('node:assert');
const path = require('node:path');
const { clearCache } = require('node:module');

const fixture = path.join(
  __dirname,
  '..',
  'fixtures',
  'source-map',
  'cjs-closure-source-map.js',
);

const { crash } = require(fixture);

const result = clearCache(fixture);
assert.strictEqual(result.commonjs, true);
assert.strictEqual(result.module, false);

try {
  crash();
  assert.fail('Expected crash() to throw');
} catch (err) {
  assert.match(err.stack, /cjs-closure-source-map-original\.js/);
}
