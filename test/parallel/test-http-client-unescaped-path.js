'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

for (var path of ['bad path', 'line\nfeed', 'carriage\rreturn', 'tab\t']) {
  assert.throws(
    function() { http.get({ path: path }, assert.fail); },
    /contains unescaped characters/,
    'Path "' + path + '" with whitespace should throw'
  );
}

for (var path of ['caf\u00e9', '\u0649\u0644\u0649', '💩' /* U+1F4A9 */ ]) {
  assert.throws(
    function() { http.get({ path: path }, assert.fail); },
    /contains unescaped characters/,
    'Path "' + path + '" with non-ASCII characters should throw'
  );
}
