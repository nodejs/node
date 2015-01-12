var http = require('http'),
    https = require('https'),
    fs = require('fs'),
    common = require('../common'),
    assert = require('assert'),
    options = {
      key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
      cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
    },
    httpServer = http.createServer(reqHandler),
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

testHttp();

function testHttp() {

  console.log('testing http on port ' + common.PORT);

  var counter = 0;

  function cb(res) {
    counter--;
    console.log('back from http request. counter = ' + counter);
    if (counter === 0) {
      httpServer.close();
      testHttps();
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
