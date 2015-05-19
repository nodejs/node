'use strict';
var http = require('http'),
    common = require('../common'),
    assert = require('assert'),
    httpServer = http.createServer(reqHandler);

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

testHttp();

function testHttp() {

  console.log('testing http on port ' + common.PORT);

  var counter = 0;

  function cb(res) {
    counter--;
    console.log('back from http request. counter = ' + counter);
    if (counter === 0) {
      httpServer.close();
    }
    res.resume();
  }

  httpServer.listen(common.PORT, function(er) {
    console.error('listening on ' + common.PORT);

    if (er) throw er;

    http.get({
      method: 'GET',
      path: '/' + (counter++),
      host: 'localhost',
      //agent: false,
      port: common.PORT,
      rejectUnauthorized: false
    }, cb).on('error', thrower);

    http.request({
      method: 'GET',
      path: '/' + (counter++),
      host: 'localhost',
      //agent: false,
      port: common.PORT,
      rejectUnauthorized: false
    }, cb).on('error', thrower).end();

    http.request({
      method: 'POST',
      path: '/' + (counter++),
      host: 'localhost',
      //agent: false,
      port: common.PORT,
      rejectUnauthorized: false
    }, cb).on('error', thrower).end();

    http.request({
      method: 'PUT',
      path: '/' + (counter++),
      host: 'localhost',
      //agent: false,
      port: common.PORT,
      rejectUnauthorized: false
    }, cb).on('error', thrower).end();

    http.request({
      method: 'DELETE',
      path: '/' + (counter++),
      host: 'localhost',
      //agent: false,
      port: common.PORT,
      rejectUnauthorized: false
    }, cb).on('error', thrower).end();
  });
}
