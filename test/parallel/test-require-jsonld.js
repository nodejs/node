'use strict';
require('../common');
const assert = require('assert');

assert.throws(
  function() {
    require('../fixtures/invalid.jsonld');
  },
  /test[\/\\]fixtures[\/\\]invalid.jsonld: Unexpected string/,
  'require() jsonld error should include path'
);

var ld = require('../fixtures/elementary.jsonld');
assert.strictEqual(ld['@id'], 'moo', 'require() loads jsonld');
