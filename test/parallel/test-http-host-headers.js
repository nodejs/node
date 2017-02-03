'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');
const httpServer = http.createServer(reqHandler);

function reqHandler(req, res) {
  if (req.url === '/setHostFalse5') {
    assert.strictEqual(req.headers.host, undefined);
  } else {
    assert.strictEqual(req.headers.host, `localhost:${this.address().port}`,
                       'Wrong host header for req[' + req.url + ']: ' +
                 req.headers.host);
  }
  res.writeHead(200, {});
  res.end('ok');
}

testHttp();

function testHttp() {

  let counter = 0;

  function cb(res) {
    counter--;
    if (counter === 0) {
      httpServer.close();
    }
    res.resume();
  }

  httpServer.listen(0, (er) => {
    assert.ifError(er);
    http.get({
      method: 'GET',
      path: '/' + (counter++),
      host: 'localhost',
      port: httpServer.address().port,
      rejectUnauthorized: false
    }, cb).on('error', common.mustNotCall());

    http.request({
      method: 'GET',
      path: '/' + (counter++),
      host: 'localhost',
      port: httpServer.address().port,
      rejectUnauthorized: false
    }, cb).on('error', common.mustNotCall()).end();

    http.request({
      method: 'POST',
      path: '/' + (counter++),
      host: 'localhost',
      port: httpServer.address().port,
      rejectUnauthorized: false
    }, cb).on('error', common.mustNotCall()).end();

    http.request({
      method: 'PUT',
      path: '/' + (counter++),
      host: 'localhost',
      port: httpServer.address().port,
      rejectUnauthorized: false
    }, cb).on('error', common.mustNotCall()).end();

    http.request({
      method: 'DELETE',
      path: '/' + (counter++),
      host: 'localhost',
      port: httpServer.address().port,
      rejectUnauthorized: false
    }, cb).on('error', common.mustNotCall()).end();
  });
}
