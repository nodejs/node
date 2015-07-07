'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var https = require('https');

var fs = require('fs'),
    options = {
      key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
      cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
    },
    httpsServer = https.createServer(options, reqHandler);

function reqHandler(req, res) {
  console.log('Got request: ' + req.headers.host + ' ' + req.url);
  if (req.url === '/setHostFalse5') {
    assert.equal(req.headers.host, undefined);
  } else {
    assert.equal(req.headers.host, 'localhost:' + common.PORT,
                 'Wrong host header for req[' + req.url + ']: ' +
                 req.headers.host);
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

  console.log('testing https on port ' + common.PORT);

  var counter = 0;

  function cb(res) {
    counter--;
    console.log('back from https request. counter = ' + counter);
    if (counter === 0) {
      httpsServer.close();
      console.log('ok');
    }
    res.resume();
  }

  httpsServer.listen(common.PORT, function(er) {
    if (er) throw er;

    https.get({
      method: 'GET',
      path: '/' + (counter++),
      host: 'localhost',
      //agent: false,
      port: common.PORT,
      rejectUnauthorized: false
    }, cb).on('error', thrower);

    https.request({
      method: 'GET',
      path: '/' + (counter++),
      host: 'localhost',
      //agent: false,
      port: common.PORT,
      rejectUnauthorized: false
    }, cb).on('error', thrower).end();

    https.request({
      method: 'POST',
      path: '/' + (counter++),
      host: 'localhost',
      //agent: false,
      port: common.PORT,
      rejectUnauthorized: false
    }, cb).on('error', thrower).end();

    https.request({
      method: 'PUT',
      path: '/' + (counter++),
      host: 'localhost',
      //agent: false,
      port: common.PORT,
      rejectUnauthorized: false
    }, cb).on('error', thrower).end();

    https.request({
      method: 'DELETE',
      path: '/' + (counter++),
      host: 'localhost',
      //agent: false,
      port: common.PORT,
      rejectUnauthorized: false
    }, cb).on('error', thrower).end();

    https.get({
      method: 'GET',
      path: '/setHostFalse' + (counter++),
      host: 'localhost',
      setHost: false,
      port: common.PORT,
      rejectUnauthorized: false
    }, cb).on('error', thrower).end();
  });
}
