// Flags: --enable-source-maps
'use strict';

require('../common');

const assert = require('node:assert');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { clearCache } = require('node:module');

const fixture = path.join(
  __dirname,
  '..',
  'fixtures',
  'source-map',
  'cjs-closure-source-map.js',
);

const { crash } = require(fixture);

clearCache(fixture, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
});

assert.strictEqual(require.cache[fixture], undefined);

try {
  crash();
  assert.fail('Expected crash() to throw');
} catch (err) {
  assert.match(err.stack, /cjs-closure-source-map-original\.js/);
}
