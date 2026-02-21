'use strict';
require('../../../common');
const assert = require('assert');
const path = require('path');

// By default, resolving 'dep' should return
// fixturesDir/resolve-paths/default/node_modules/dep/index.js. By setting
// the path to fixturesDir/resolve-paths/default, the 'default' directory
// structure should be ignored.

assert.strictEqual(
  require.resolve('dep'),
  path.join(__dirname, 'node_modules', 'dep', 'index.js')
);

const paths = [path.resolve(__dirname, '..', 'defined')];

assert.strictEqual(
  require.resolve('dep', { paths }),
  path.join(paths[0], 'node_modules', 'dep', 'index.js')
);
