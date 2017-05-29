
/**
 * Module dependencies.
 */

var fs = require('fs');
var url = require('url');
var http = require('http');
var https = require('https');
var assert = require('assert');
var socks = require('socksv5');
var SocksProxyAgent = require('../');

describe('SocksProxyAgent', function () {
  var httpServer, httpPort;
  var httpsServer, httpsPort;
  var socksServer, socksPort;

  before(function (done) {
    // setup SOCKS proxy server
    socksServer = socks.createServer(function(info, accept, deny) {
      accept();
    });
    socksServer.listen(0, '127.0.0.1', function() {
      socksPort = socksServer.address().port;
      //console.log('SOCKS server listening on port %d', socksPort);
      done();
    });
    socksServer.useAuth(socks.auth.None());
    //socksServer.useAuth(socks.auth.UserPassword(function(user, password, cb) {
    //  cb(user === 'nodejs' && password === 'rules!');
    //}));
  });

  before(function (done) {
    // setup target HTTP server
    httpServer = http.createServer();
    httpServer.listen(function () {
      httpPort = httpServer.address().port;
      done();
    });
  });

  before(function (done) {
    // setup target SSL HTTPS server
    var options = {
      key: fs.readFileSync(__dirname + '/ssl-cert-snakeoil.key'),
      cert: fs.readFileSync(__dirname + '/ssl-cert-snakeoil.pem')
    };
    httpsServer = https.createServer(options);
    httpsServer.listen(function () {
      httpsPort = httpsServer.address().port;
      done();
    });
  });

  after(function (done) {
    socksServer.once('close', function () { done(); });
    socksServer.close();
  });

  after(function (done) {
    httpServer.once('close', function () { done(); });
    httpServer.close();
  });

  after(function (done) {
    httpsServer.once('close', function () { done(); });
    httpsServer.close();
  });

  describe('constructor', function () {
    it('should throw an Error if no "proxy" argument is given', function () {
      assert.throws(function () {
        new SocksProxyAgent();
      });
    });
    it('should accept a "string" proxy argument', function () {
      var agent = new SocksProxyAgent('socks://127.0.0.1:' + socksPort);
      assert.equal('127.0.0.1', agent.proxy.host);
      assert.equal(socksPort, agent.proxy.port);
    });
    it('should accept a `url.parse()` result object argument', function () {
      var opts = url.parse('socks://127.0.0.1:' + socksPort);
      var agent = new SocksProxyAgent(opts);
      assert.equal('127.0.0.1', agent.proxy.host);
      assert.equal(socksPort, agent.proxy.port);
    });
  });

  describe('"http" module', function () {
    it('should work against an HTTP endpoint', function (done) {
      httpServer.once('request', function (req, res) {
        assert.equal('/foo', req.url);
        res.statusCode = 404;
        res.end(JSON.stringify(req.headers));
      });

      var agent = new SocksProxyAgent('socks://127.0.0.1:' + socksPort);
      var opts = url.parse('http://127.0.0.1:' + httpPort + '/foo');
      opts.agent = agent;
      opts.headers = { foo: 'bar' };
      var req = http.get(opts, function (res) {
        assert.equal(404, res.statusCode);
        var data = '';
        res.setEncoding('utf8');
        res.on('data', function (b) {
          data += b;
        });
        res.on('end', function () {
          data = JSON.parse(data);
          assert.equal('bar', data.foo);
          done();
        });
      });
      req.once('error', done);
    });
  });

  describe('"https" module', function () {
    it('should work against an HTTPS endpoint', function (done) {
      httpsServer.once('request', function (req, res) {
        assert.equal('/foo', req.url);
        res.statusCode = 404;
        res.end(JSON.stringify(req.headers));
      });

      var agent = new SocksProxyAgent('socks://127.0.0.1:' + socksPort);
      var opts = url.parse('https://127.0.0.1:' + httpsPort + '/foo');
      opts.agent = agent;
      opts.rejectUnauthorized = false;

      opts.headers = { foo: 'bar' };
      var req = https.get(opts, function (res) {
        assert.equal(404, res.statusCode);
        var data = '';
        res.setEncoding('utf8');
        res.on('data', function (b) {
          data += b;
        });
        res.on('end', function () {
          data = JSON.parse(data);
          assert.equal('bar', data.foo);
          done();
        });
      });
      req.once('error', done);
    });
  });

});
