'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');

const assert = require('assert');
const { pathToFileURL } = require('url');
const { Worker } = require('worker_threads');

new Worker(new URL(
  'data:text/javascript,' +
  `import ${JSON.stringify(pathToFileURL(fixtures.path('altdocs.md')))};`
)).on('error', common.mustCall((err) => {
  assert.strictEqual(err.code, 'ERR_UNKNOWN_FILE_EXTENSION');
}));
