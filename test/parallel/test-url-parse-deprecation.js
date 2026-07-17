'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');

const assert = require('assert');
const url = require('url');

const fixturePath = fixtures.path('node_modules', 'url-deprecations.js');

// Application deprecation for url.parse()
url.parse('foo');
common.expectWarning('DeprecationWarning',
                     [/`url\.parse\(\)` behavior is not standardized/, 'DEP0169']);

// No warnings from inside node_modules
common.spawnPromisified(process.execPath, [fixturePath]).then(
  common.mustCall(({ code, signal, stderr }) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.doesNotMatch(stderr, /\[DEP0169\]/);
  })
);
