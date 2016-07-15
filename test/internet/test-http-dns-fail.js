'use strict';
/*
 * Repeated requests for a domain that fails to resolve
 * should trigger the error event after each attempt.
 */

const common = require('../common');
var assert = require('assert');
var http = require('http');

var hadError = 0;

function httpreq(count) {
  if (1 < count) return;

  var req = http.request({
    host: 'not-a-real-domain-name.nobody-would-register-this-as-a-tld',
    port: 80,
    path: '/',
    method: 'GET'
  }, common.fail);

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
  assert.equal(2, hadError);
});
