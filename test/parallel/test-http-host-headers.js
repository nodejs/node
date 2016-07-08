'use strict';
require('../common');
const http = require('http');
const assert = require('assert');
const httpServer = http.createServer(reqHandler);

function reqHandler(req, res) {
  console.log('Got request: ' + req.headers.host + ' ' + req.url);
  if (req.url === '/setHostFalse5') {
    assert.equal(req.headers.host, undefined);
  } else {
    assert.equal(req.headers.host, `localhost:${this.address().port}`,
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

testHttp();

function testHttp() {

  var counter = 0;

  function cb(res) {
    counter--;
    console.log('back from http request. counter = ' + counter);
    if (counter === 0) {
      httpServer.close();
    }
    res.resume();
  }

  httpServer.listen(0, function(er) {
    console.error(`test http server listening on ${this.address().port}`);

    if (er) throw er;

    http.get({
      method: 'GET',
      path: '/' + (counter++),
      host: 'localhost',
      //agent: false,
      port: this.address().port,
      rejectUnauthorized: false
    }, cb).on('error', thrower);

    http.request({
      method: 'GET',
      path: '/' + (counter++),
      host: 'localhost',
      //agent: false,
      port: this.address().port,
      rejectUnauthorized: false
    }, cb).on('error', thrower).end();

    http.request({
      method: 'POST',
      path: '/' + (counter++),
      host: 'localhost',
      //agent: false,
      port: this.address().port,
      rejectUnauthorized: false
    }, cb).on('error', thrower).end();

    http.request({
      method: 'PUT',
      path: '/' + (counter++),
      host: 'localhost',
      //agent: false,
      port: this.address().port,
      rejectUnauthorized: false
    }, cb).on('error', thrower).end();

    http.request({
      method: 'DELETE',
      path: '/' + (counter++),
      host: 'localhost',
      //agent: false,
      port: this.address().port,
      rejectUnauthorized: false
    }, cb).on('error', thrower).end();
  });
}
