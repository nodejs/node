'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

assert.throws(function() {
  http.get({ path: 'bad path' }, assert.fail);
}, /contains unescaped characters/, 'Paths with spaces in it should throw');

for (var path of ['caf\u00e9', '\u0649\u0644\u0649', '💩' /* U+1F4A9 */ ]) {
  assert.throws(
    function() { http.get({ path: 'caf\u00e9' }, assert.fail); },
    /contains unescaped characters/,
    'Path ' + path + ' with non-ASCII characters should throw'
  );
}
