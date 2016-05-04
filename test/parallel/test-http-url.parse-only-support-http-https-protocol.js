'use strict';
require('../common');
var assert = require('assert');
var http = require('http');
var url = require('url');

function errChecker(protocol, expected) {
  return function(err) {
    if (err instanceof Error) {
      console.log(err);
      assert.strictEqual(err.code, 'HTTPUNSUPPORTEDPROTOCOL');
      assert.strictEqual(err.name, 'Error[HTTPUNSUPPORTEDPROTOCOL]');
      assert.strictEqual(
          err.message,
          `Protocol "${protocol}" is not supported. Expected "${expected}"`);
      return true;
    }
  };
}

[
  ['file:', 'file:///whatever'],
  ['mailto:', 'mailto:asdf@asdf.com'],
  ['ftp:', 'ftp://www.example.org'],
  ['javascript:', 'javascript:alert(\'hello\');'],
  ['xmpp:', 'xmpp:example@jabber.org'],
  ['f:', 'f://some.host/path']
].forEach((item) => {
  assert.throws(function() {
    http.request(url.parse(item[1]));
  }, errChecker(item[0], 'http:'));
});
