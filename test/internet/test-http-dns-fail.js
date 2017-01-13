'use strict';
/*
 * Repeated requests for a domain that fails to resolve
 * should trigger the error event after each attempt.
 */

const common = require('../common');
const assert = require('assert');
const http = require('http');

function httpreq(count) {
  if (count > 1) return;

  const req = http.request({
    host: 'not-a-real-domain-name.nobody-would-register-this-as-a-tld',
    port: 80,
    path: '/',
    method: 'GET'
  }, common.fail);

  req.on('error', common.mustCall((e) => {
    assert.strictEqual(e.code, 'ENOTFOUND');
    httpreq(count + 1);
  }));

  req.end();
}

httpreq(0);
