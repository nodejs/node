
/**
 * Module dependencies.
 */

var fs = require('fs');
var url = require('url');
var net = require('net');
var tls = require('tls');
var http = require('http');
var https = require('https');
var assert = require('assert');
var events = require('events');
var Agent = require('../');

describe('Agent', function () {
  describe('"error" event', function () {
    it('should be invoked on `http.ClientRequest` instance if passed to callback function on the first tick', function (done) {
      var agent = new Agent(function (req, opts, fn) {
        fn(new Error('is this caught?'));
      });
      var info = url.parse('http://127.0.0.1/foo');
      info.agent = agent;
      var req = http.get(info);
      req.on('error', function (err) {
        assert.equal('is this caught?', err.message);
        done();
      });
    });
    it('should be invoked on `http.ClientRequest` instance if passed to callback function after the first tick', function (done) {
      var agent = new Agent(function (req, opts, fn) {
        setTimeout(function () {
          fn(new Error('is this caught?'));
        }, 10);
      });
      var info = url.parse('http://127.0.0.1/foo');
      info.agent = agent;
      var req = http.get(info);
      req.on('error', function (err) {
        assert.equal('is this caught?', err.message);
        done();
      });
    });
  });
  describe('artificial "streams"', function () {
    it('should send a GET request', function (done) {
      var stream = new events.EventEmitter();

      // needed for the `http` module to call .write() on the stream
      stream.writable = true;

      stream.write = function (str) {
        assert(0 == str.indexOf('GET / HTTP/1.1'));
        done();
      };

      var opts = {
        method: 'GET',
        host: '127.0.0.1',
        path: '/',
        port: 80,
        agent: new Agent(function (req, opts, fn) {
          fn(null, stream);
        })
      };
      var req = http.request(opts);
      req.end();
    });
    it('should receive a GET response', function (done) {
      var stream = new events.EventEmitter();
      var opts = {
        method: 'GET',
        host: '127.0.0.1',
        path: '/',
        port: 80,
        agent: new Agent(function (req, opts, fn) {
          fn(null, stream);
        })
      };
      var req = http.request(opts, function (res) {
        assert.equal('0.9', res.httpVersion);
        assert.equal(111, res.statusCode);
        assert.equal('bar', res.headers.foo);
        done();
      });
      req.end();

      // have to nextTick() since `http.ClientRequest` doesn't *actually*
      // attach the listeners to the "stream" until the next tick :\
      process.nextTick(function () {
        var buf = new Buffer('HTTP/0.9 111\r\n' +
                             'Foo: bar\r\n' +
                             'Set-Cookie: 1\r\n' +
                             'Set-Cookie: 2\r\n\r\n');
        if ('function' == typeof stream.ondata) {
          // node <= v0.11.3
          stream.ondata(buf, 0, buf.length);
        } else {
          // node > v0.11.3
          stream.emit('data', buf);
        }
      });
    });
  });
});

describe('"http" module', function () {
  var server;
  var port;

  // setup test HTTP server
  before(function (done) {
    server = http.createServer();
    server.listen(0, function () {
      port = server.address().port;
      done();
    });
  });

  // shut down test HTTP server
  after(function (done) {
    server.once('close', function () {
      done();
    });
    server.close();
  });

  it('should work for basic HTTP requests', function (done) {
    var called = false;
    var agent = new Agent(function (req, opts, fn) {
      called = true;
      var socket = net.connect(opts);
      fn(null, socket);
    });

    // add HTTP server "request" listener
    var gotReq = false;
    server.once('request', function (req, res) {
      gotReq = true;
      res.setHeader('X-Foo', 'bar');
      res.setHeader('X-Url', req.url);
      res.end();
    });

    var info = url.parse('http://127.0.0.1:' + port + '/foo');
    info.agent = agent;
    http.get(info, function (res) {
      assert.equal('bar', res.headers['x-foo']);
      assert.equal('/foo', res.headers['x-url']);
      assert(gotReq);
      assert(called);
      done();
    });
  });

  it('should set the `Connection: close` response header', function (done) {
    var called = false;
    var agent = new Agent(function (req, opts, fn) {
      called = true;
      var socket = net.connect(opts);
      fn(null, socket);
    });

    // add HTTP server "request" listener
    var gotReq = false;
    server.once('request', function (req, res) {
      gotReq = true;
      res.setHeader('X-Url', req.url);
      assert.equal('close', req.headers.connection);
      res.end();
    });

    var info = url.parse('http://127.0.0.1:' + port + '/bar');
    info.agent = agent;
    http.get(info, function (res) {
      assert.equal('/bar', res.headers['x-url']);
      assert.equal('close', res.headers.connection);
      assert(gotReq);
      assert(called);
      done();
    });
  });

  it('should pass through options from `http.request()`', function (done) {
    var agent = new Agent(function (req, opts, fn) {
      assert.equal('google.com', opts.host);
      assert.equal('bar', opts.foo);
      done();
    });

    http.get({
      host: 'google.com',
      foo: 'bar',
      agent: agent
    });
  });

  it('should default to port 80', function (done) {
    var agent = new Agent(function (req, opts, fn) {
      assert.equal(80, opts.port);
      done();
    });

    // (probably) not hitting a real HTTP server here,
    // so no need to add a httpServer request listener
    http.get({
      host: '127.0.0.1',
      path: '/foo',
      agent: agent
    });
  });
});

describe('"https" module', function () {
  var server;
  var port;

  // setup test HTTPS server
  before(function (done) {
    var options = {
      key: fs.readFileSync(__dirname + '/ssl-cert-snakeoil.key'),
      cert: fs.readFileSync(__dirname + '/ssl-cert-snakeoil.pem')
    };
    server = https.createServer(options);
    server.listen(0, function () {
      port = server.address().port;
      done();
    });
  });

  // shut down test HTTP server
  after(function (done) {
    server.once('close', function () {
      done();
    });
    server.close();
  });

  it('should work for basic HTTPS requests', function (done) {
    var called = false;
    var agent = new Agent(function (req, opts, fn) {
      called = true;
      assert(opts.secureEndpoint);
      var socket = tls.connect(opts);
      fn(null, socket);
    });

    // add HTTPS server "request" listener
    var gotReq = false;
    server.once('request', function (req, res) {
      gotReq = true;
      res.setHeader('X-Foo', 'bar');
      res.setHeader('X-Url', req.url);
      res.end();
    });

    var info = url.parse('https://127.0.0.1:' + port + '/foo');
    info.agent = agent;
    info.rejectUnauthorized = false;
    https.get(info, function (res) {
      assert.equal('bar', res.headers['x-foo']);
      assert.equal('/foo', res.headers['x-url']);
      assert(gotReq);
      assert(called);
      done();
    });
  });

  it('should pass through options from `https.request()`', function (done) {
    var agent = new Agent(function (req, opts, fn) {
      assert.equal('google.com', opts.host);
      assert.equal('bar', opts.foo);
      done();
    });

    https.get({
      host: 'google.com',
      foo: 'bar',
      agent: agent
    });
  });

  it('should default to port 443', function (done) {
    var agent = new Agent(function (req, opts, fn) {
      assert.equal(true, opts.secureEndpoint);
      assert.equal(false, opts.rejectUnauthorized);
      assert.equal(443, opts.port);
      done();
    });

    // (probably) not hitting a real HTTPS server here,
    // so no need to add a httpsServer request listener
    https.get({
      host: '127.0.0.1',
      path: '/foo',
      agent: agent,
      rejectUnauthorized: false
    });
  });
});
