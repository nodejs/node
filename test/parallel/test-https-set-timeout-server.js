var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var https = require('https');

var tls = require('tls');
var fs = require('fs');

var tests = [];

var serverOptions = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

function test(fn) {
  if (!tests.length)
    process.nextTick(run);
  tests.push(fn);
}

function run() {
  var fn = tests.shift();
  if (fn) {
    console.log('# %s', fn.name);
    fn(run);
  } else
    console.log('ok');
}

test(function serverTimeout(cb) {
  var caughtTimeout = false;
  process.on('exit', function() {
    assert(caughtTimeout);
  });
  var server = https.createServer(serverOptions, function(req, res) {
    // just do nothing, we should get a timeout event.
  });
  server.listen(common.PORT);
  server.setTimeout(50, function(socket) {
    caughtTimeout = true;
    socket.destroy();
    server.close();
    cb();
  });
  https.get({
    port: common.PORT,
    rejectUnauthorized: false
  }).on('error', function() {});
});

test(function serverRequestTimeout(cb) {
  var caughtTimeout = false;
  process.on('exit', function() {
    assert(caughtTimeout);
  });
  var server = https.createServer(serverOptions, function(req, res) {
    // just do nothing, we should get a timeout event.
    req.setTimeout(50, function() {
      caughtTimeout = true;
      req.socket.destroy();
      server.close();
      cb();
    });
  });
  server.listen(common.PORT);
  var req = https.request({
    port: common.PORT,
    method: 'POST',
    rejectUnauthorized: false
  });
  req.on('error', function() {});
  req.write('Hello');
  // req is in progress
});

test(function serverResponseTimeout(cb) {
  var caughtTimeout = false;
  process.on('exit', function() {
    assert(caughtTimeout);
  });
  var server = https.createServer(serverOptions, function(req, res) {
    // just do nothing, we should get a timeout event.
    res.setTimeout(50, function() {
      caughtTimeout = true;
      res.socket.destroy();
      server.close();
      cb();
    });
  });
  server.listen(common.PORT);
  https.get({
    port: common.PORT,
    rejectUnauthorized: false
  }).on('error', function() {});
});

test(function serverRequestNotTimeoutAfterEnd(cb) {
  var caughtTimeoutOnRequest = false;
  var caughtTimeoutOnResponse = false;
  process.on('exit', function() {
    assert(!caughtTimeoutOnRequest);
    assert(caughtTimeoutOnResponse);
  });
  var server = https.createServer(serverOptions, function(req, res) {
    // just do nothing, we should get a timeout event.
    req.setTimeout(50, function(socket) {
      caughtTimeoutOnRequest = true;
    });
    res.on('timeout', function(socket) {
      caughtTimeoutOnResponse = true;
    });
  });
  server.on('timeout', function(socket) {
    socket.destroy();
    server.close();
    cb();
  });
  server.listen(common.PORT);
  https.get({
    port: common.PORT,
    rejectUnauthorized: false
  }).on('error', function() {});
});

test(function serverResponseTimeoutWithPipeline(cb) {
  var caughtTimeout = '';
  process.on('exit', function() {
    assert.equal(caughtTimeout, '/2');
  });
  var server = https.createServer(serverOptions, function(req, res) {
    res.setTimeout(50, function() {
      caughtTimeout += req.url;
    });
    if (req.url === '/1') res.end();
  });
  server.on('timeout', function(socket) {
    socket.destroy();
    server.close();
    cb();
  });
  server.listen(common.PORT);
  var options = {
    port: common.PORT,
    allowHalfOpen: true,
    rejectUnauthorized: false
  };
  var c = tls.connect(options, function() {
    c.write('GET /1 HTTP/1.1\r\nHost: localhost\r\n\r\n');
    c.write('GET /2 HTTP/1.1\r\nHost: localhost\r\n\r\n');
    c.write('GET /3 HTTP/1.1\r\nHost: localhost\r\n\r\n');
  });
});

test(function idleTimeout(cb) {
  var caughtTimeoutOnRequest = false;
  var caughtTimeoutOnResponse = false;
  var caughtTimeoutOnServer = false;
  process.on('exit', function() {
    assert(!caughtTimeoutOnRequest);
    assert(!caughtTimeoutOnResponse);
    assert(caughtTimeoutOnServer);
  });
  var server = https.createServer(serverOptions, function(req, res) {
    req.on('timeout', function(socket) {
      caughtTimeoutOnRequest = true;
    });
    res.on('timeout', function(socket) {
      caughtTimeoutOnResponse = true;
    });
    res.end();
  });
  server.setTimeout(50, function(socket) {
    caughtTimeoutOnServer = true;
    socket.destroy();
    server.close();
    cb();
  });
  server.listen(common.PORT);
  var options = {
    port: common.PORT,
    allowHalfOpen: true,
    rejectUnauthorized: false
  };
  tls.connect(options, function() {
    this.write('GET /1 HTTP/1.1\r\nHost: localhost\r\n\r\n');
    // Keep-Alive
  });
});
