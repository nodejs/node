'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const debug = require('util').debuglog('test');

let counter = 0;

const httpsServer = https.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
}, common.mustCall(function(req, res) {
  debug(`Got request: ${req.headers.host} ${req.url}`);
  if (req.url.startsWith('/setHostFalse')) {
    assert.strictEqual(req.headers.host, undefined);
  } else {
    assert.strictEqual(
      req.headers.host, `localhost:${this.address().port}`,
      `Wrong host header for req[${req.url}]: ${req.headers.host}`);
  }
  res.writeHead(200, {});
  res.end('ok');
}, 6)).listen(0, common.mustCall(function(err) {
  debug(`test https server listening on port ${this.address().port}`);
  assert.ifError(err);
  https.get({
    method: 'GET',
    path: `/${counter++}`,
    host: 'localhost',
    port: this.address().port,
    rejectUnauthorized: false,
  }, cb).on('error', common.mustNotCall());

  https.request({
    method: 'GET',
    path: `/${counter++}`,
    host: 'localhost',
    port: this.address().port,
    rejectUnauthorized: false,
  }, cb).on('error', common.mustNotCall()).end();

  https.request({
    method: 'POST',
    path: `/${counter++}`,
    host: 'localhost',
    port: this.address().port,
    rejectUnauthorized: false,
  }, cb).on('error', common.mustNotCall()).end();

  https.request({
    method: 'PUT',
    path: `/${counter++}`,
    host: 'localhost',
    port: this.address().port,
    rejectUnauthorized: false,
  }, cb).on('error', common.mustNotCall()).end();

  https.request({
    method: 'DELETE',
    path: `/${counter++}`,
    host: 'localhost',
    port: this.address().port,
    rejectUnauthorized: false,
  }, cb).on('error', common.mustNotCall()).end();

  https.get({
    method: 'GET',
    path: `/setHostFalse${counter++}`,
    host: 'localhost',
    setHost: false,
    port: this.address().port,
    rejectUnauthorized: false,
  }, cb).on('error', common.mustNotCall());

  https.request({
    method: 'GET',
    path: `/${counter++}`,
    host: 'localhost',
    setHost: true,
    // agent: false,
    port: this.address().port,
    rejectUnauthorized: false,
  }, cb).on('error', common.mustNotCall()).end();

  https.get({
    method: 'GET',
    path: `/setHostFalse${counter++}`,
    host: 'localhost',
    setHost: 0,
    port: this.address().port,
    rejectUnauthorized: false,
  }, cb).on('error', common.mustNotCall());

  https.get({
    method: 'GET',
    path: `/setHostFalse${counter++}`,
    host: 'localhost',
    setHost: null,
    port: this.address().port,
    rejectUnauthorized: false,
  }, cb).on('error', common.mustNotCall());
}));

const cb = common.mustCall(function(res) {
  counter--;
  debug(`back from https request. counter = ${counter}`);
  if (counter === 0) {
    httpsServer.close();
    debug('ok');
  }
  res.resume();
}, 9);
