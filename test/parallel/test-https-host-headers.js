'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};
const httpsServer = https.createServer(options, reqHandler);

function reqHandler(req, res) {
  console.log(`Got request: ${req.headers.host} ${req.url}`);
  if (req.url === '/setHostFalse5') {
    assert.strictEqual(req.headers.host, undefined);
  } else {
    assert.strictEqual(
      req.headers.host, `localhost:${this.address().port}`,
      `Wrong host header for req[${req.url}]: ${req.headers.host}`);
  }
  res.writeHead(200, {});
  //process.nextTick(function() { res.end('ok'); });
  res.end('ok');
}

function thrower(er) {
  throw er;
}

testHttps();

function testHttps() {

  let counter = 0;

  function cb(res) {
    counter--;
    console.log(`back from https request. counter = ${counter}`);
    if (counter === 0) {
      httpsServer.close();
      console.log('ok');
    }
    res.resume();
  }

  httpsServer.listen(0, function(er) {
    console.log(`test https server listening on port ${this.address().port}`);
    assert.ifError(er);
    https.get({
      method: 'GET',
      path: `/${counter++}`,
      host: 'localhost',
      //agent: false,
      port: this.address().port,
      rejectUnauthorized: false
    }, cb).on('error', thrower);

    https.request({
      method: 'GET',
      path: `/${counter++}`,
      host: 'localhost',
      //agent: false,
      port: this.address().port,
      rejectUnauthorized: false
    }, cb).on('error', thrower).end();

    https.request({
      method: 'POST',
      path: `/${counter++}`,
      host: 'localhost',
      //agent: false,
      port: this.address().port,
      rejectUnauthorized: false
    }, cb).on('error', thrower).end();

    https.request({
      method: 'PUT',
      path: `/${counter++}`,
      host: 'localhost',
      //agent: false,
      port: this.address().port,
      rejectUnauthorized: false
    }, cb).on('error', thrower).end();

    https.request({
      method: 'DELETE',
      path: `/${counter++}`,
      host: 'localhost',
      //agent: false,
      port: this.address().port,
      rejectUnauthorized: false
    }, cb).on('error', thrower).end();

    https.get({
      method: 'GET',
      path: `/setHostFalse${counter++}`,
      host: 'localhost',
      setHost: false,
      port: this.address().port,
      rejectUnauthorized: false
    }, cb).on('error', thrower).end();
  });
}
