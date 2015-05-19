'use strict';
/*
 * Repeated requests for a domain that fails to resolve
 * should trigger the error event after each attempt.
 */

var common = require('../common');
var assert = require('assert');
var http = require('http');

var resDespiteError = false;
var hadError = 0;

function httpreq(count) {
  if (1 < count) return;

  var req = http.request({
    host: 'not-a-real-domain-name.nobody-would-register-this-as-a-tld',
    port: 80,
    path: '/',
    method: 'GET'
  }, function(res) {
    resDespiteError = true;
  });

  req.on('error', function(e) {
    console.log(e.message);
    assert.strictEqual(e.code, 'ENOTFOUND');
    hadError++;
    httpreq(count + 1);
  });

  req.end();
}

httpreq(0);


process.on('exit', function() {
  assert.equal(false, resDespiteError);
  assert.equal(2, hadError);
});
