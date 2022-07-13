'use strict';

require('../common');
const fixtures = require('../common/fixtures');
const path = require('node:path');
const assert = require('node:assert');

// should not throw
require(path.resolve(fixtures.path('/cjs-loader-node_ceiling/nested-without-node_ceiling/find-dep.js')));
require(path.resolve(fixtures.path('/cjs-loader-node_ceiling/find-dep.js')));

assert.throws(
  () => {
    require(path.resolve(fixtures.path('/cjs-loader-node_ceiling/nested-with-node_ceiling/dep-not-found.js')));
  }, {
    code: 'MODULE_NOT_FOUND',
  }
);

assert.throws(
  () => {
    require(path.resolve(fixtures.path(
      '/cjs-loader-node_ceiling/package-not-found-due-to-node_ceiling/dir-with-node_ceiling/dep-not-found.js'
    )));
  }, {
    code: 'MODULE_NOT_FOUND',
  }
);
