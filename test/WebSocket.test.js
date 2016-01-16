var assert = require('assert')
  , https = require('https')
  , http = require('http')
  , should = require('should')
  , WebSocket = require('../')
  , WebSocketServer = require('../').Server
  , fs = require('fs')
  , os = require('os')
  , server = require('./testserver')
  , crypto = require('crypto');

var port = 20000;

function getArrayBuffer(buf) {
  var l = buf.length;
  var arrayBuf = new ArrayBuffer(l);
  var uint8View = new Uint8Array(arrayBuf);
  for (var i = 0; i < l; i++) {
    uint8View[i] = buf[i];
  }
  return uint8View.buffer;
}


function areArraysEqual(x, y) {
  if (x.length != y.length) return false;
  for (var i = 0, l = x.length; i < l; ++i) {
    if (x[i] !== y[i]) return false;
  }
  return true;
}

describe('WebSocket', function() {
  describe('#ctor', function() {
    it('throws exception for invalid url', function(done) {
      try {
        var ws = new WebSocket('echo.websocket.org');
      }
      catch (e) {
        done();
      }
    });

    it('should return a new instance if called without new', function(done) {
      var ws = WebSocket('ws://localhost:' + port);
      ws.should.be.an.instanceOf(WebSocket);
      done();
    });
  });

  describe('options', function() {
    it('should accept an `agent` option', function(done) {
      var wss = new WebSocketServer({port: ++port}, function() {
        var agent = {
          addRequest: function() {
            wss.close();
            done();
          }
        };
        var ws = new WebSocket('ws://localhost:' + port, { agent: agent });
      });
    });
    // GH-227
    it('should accept the `options` object as the 3rd argument', function(done) {
      var wss = new WebSocketServer({port: ++port}, function() {
        var agent = {
          addRequest: function() {
            wss.close();
            done();
          }
        };
        var ws = new WebSocket('ws://localhost:' + port, [], { agent: agent });
      });
    });

    it('should accept the localAddress option', function(done) {
      // explore existing interfaces
      var devs = os.networkInterfaces()
        , localAddresses = []
        , j, ifc, dev, devname;
      for ( devname in devs ) {
        dev = devs[devname];
        for ( j=0;j<dev.length;j++ ) {
          ifc = dev[j];
          if ( !ifc.internal && ifc.family === 'IPv4' ) {
            localAddresses.push(ifc.address);
          }
        }
      }
      var wss = new WebSocketServer({port: ++port}, function() {
        var ws = new WebSocket('ws://localhost:' + port, { localAddress: localAddresses[0] });
        ws.on('open', function () {
          done();
        });
      });
    });

    it('should accept the localAddress option whether it was wrong interface', function(done) {
      if ( process.platform === 'linux' && process.version.match(/^v0\.([0-9]\.|10)/) ) {
        return done();
      }
      var wss = new WebSocketServer({port: ++port}, function() {
        try {
          var ws = new WebSocket('ws://localhost:' + port, { localAddress: '123.456.789.428' });
          ws.on('error', function (error) {
            error.code.should.eql('EADDRNOTAVAIL');
            done();
          });
        }
        catch(e) {
          e.should.match(/localAddress must be a valid IP/);
          done();
        }
      });
    });
  });

  describe('properties', function() {
    it('#bytesReceived exposes number of bytes received', function(done) {
      var wss = new WebSocketServer({port: ++port}, function() {
        var ws = new WebSocket('ws://localhost:' + port, { perMessageDeflate: false });
        ws.on('message', function() {
          ws.bytesReceived.should.eql(8);
          wss.close();
          done();
        });
      });
      wss.on('connection', function(ws) {
        ws.send('foobar');
      });
    });

    it('#url exposes the server url', function(done) {
      server.createServer(++port, function(srv) {
        var url = 'ws://localhost:' + port;
        var ws = new WebSocket(url);
        assert.equal(url, ws.url);
        ws.terminate();
        ws.on('close', function() {
          srv.close();
          done();
        });
      });
    });

    it('#protocolVersion exposes the protocol version', function(done) {
      server.createServer(++port, function(srv) {
        var url = 'ws://localhost:' + port;
        var ws = new WebSocket(url);
        assert.equal(13, ws.protocolVersion);
        ws.terminate();
        ws.on('close', function() {
          srv.close();
          done();
        });
      });
    });

    describe('#bufferedAmount', function() {
      it('defaults to zero', function(done) {
        server.createServer(++port, function(srv) {
          var url = 'ws://localhost:' + port;
          var ws = new WebSocket(url);
          assert.equal(0, ws.bufferedAmount);
          ws.terminate();
          ws.on('close', function() {
            srv.close();
            done();
          });
        });
      });

      it('defaults to zero upon "open"', function(done) {
        server.createServer(++port, function(srv) {
          var url = 'ws://localhost:' + port;
          var ws = new WebSocket(url);
          ws.onopen = function() {
            assert.equal(0, ws.bufferedAmount);
            ws.terminate();
            ws.on('close', function() {
              srv.close();
              done();
            });
          };
        });
      });

      it('stress kernel write buffer', function(done) {
        var wss = new WebSocketServer({port: ++port}, function() {
          var ws = new WebSocket('ws://localhost:' + port, { perMessageDeflate: false });
        });
        wss.on('connection', function(ws) {
          while (true) {
            if (ws.bufferedAmount > 0) break;
            ws.send((new Array(10000)).join('hello'));
          }
          ws.terminate();
          ws.on('close', function() {
            wss.close();
            done();
          });
        });
      });
    });

    describe('Custom headers', function() {
      it('request has an authorization header', function (done) {
        var auth = 'test:testpass';
        var srv = http.createServer(function (req, res) {});
        var wss = new WebSocketServer({server: srv});
        srv.listen(++port);
        var ws = new WebSocket('ws://' + auth + '@localhost:' + port);
        srv.on('upgrade', function (req, socket, head) {
          assert(req.headers.authorization, 'auth header exists');
          assert.equal(req.headers.authorization, 'Basic ' + new Buffer(auth).toString('base64'));
          ws.terminate();
          ws.on('close', function () {
            srv.close();
            wss.close();
            done();
          });
        });
      });

      it('accepts custom headers', function (done) {
        var srv = http.createServer(function (req, res) {});
        var wss = new WebSocketServer({server: srv});
        srv.listen(++port);

        var ws = new WebSocket('ws://localhost:' + port, {
          headers: {
            'Cookie': 'foo=bar'
          }
        });

        srv.on('upgrade', function (req, socket, head) {
          assert(req.headers.cookie, 'auth header exists');
          assert.equal(req.headers.cookie, 'foo=bar');

          ws.terminate();
          ws.on('close', function () {
            srv.close();
            wss.close();
            done();
          });
        });
      });
    });

    describe('#readyState', function() {
      it('defaults to connecting', function(done) {
        server.createServer(++port, function(srv) {
          var ws = new WebSocket('ws://localhost:' + port);
          assert.equal(WebSocket.CONNECTING, ws.readyState);
          ws.terminate();
          ws.on('close', function() {
            srv.close();
            done();
          });
        });
      });

      it('set to open once connection is established', function(done) {
        server.createServer(++port, function(srv) {
          var ws = new WebSocket('ws://localhost:' + port);
          ws.on('open', function() {
            assert.equal(WebSocket.OPEN, ws.readyState);
            srv.close();
            done();
          });
        });
      });

      it('set to closed once connection is closed', function(done) {
        server.createServer(++port, function(srv) {
          var ws = new WebSocket('ws://localhost:' + port);
          ws.close(1001);
          ws.on('close', function() {
            assert.equal(WebSocket.CLOSED, ws.readyState);
            srv.close();
            done();
          });
        });
      });

      it('set to closed once connection is terminated', function(done) {
        server.createServer(++port, function(srv) {
          var ws = new WebSocket('ws://localhost:' + port);
          ws.terminate();
          ws.on('close', function() {
            assert.equal(WebSocket.CLOSED, ws.readyState);
            srv.close();
            done();
          });
        });
      });
    });

    /*
     * Ready state constants
     */

    var readyStates = {
      CONNECTING: 0,
      OPEN: 1,
      CLOSING: 2,
      CLOSED: 3
    };

    /*
     * Ready state constant tests
     */

    Object.keys(readyStates).forEach(function(state) {
      describe('.' + state, function() {
        it('is enumerable property of class', function() {
          var propertyDescripter = Object.getOwnPropertyDescriptor(WebSocket, state)
          assert.equal(readyStates[state], propertyDescripter.value);
          assert.equal(true, propertyDescripter.enumerable);
        });
      });
    });

    server.createServer(++port, function(srv) {
      var ws = new WebSocket('ws://localhost:' + port);
      Object.keys(readyStates).forEach(function(state) {
        describe('.' + state, function() {
          it('is property of instance', function() {
            assert.equal(readyStates[state], ws[state]);
          });
        });
      });
    });
  });

  describe('events', function() {
    it('emits a ping event', function(done) {
      var wss = new WebSocketServer({port: ++port});
      wss.on('connection', function(client) {
        client.ping();
      });
      var ws = new WebSocket('ws://localhost:' + port);
      ws.on('ping', function() {
        ws.terminate();
        wss.close();
        done();
      });
    });

    it('emits a pong event', function(done) {
      var wss = new WebSocketServer({port: ++port});
      wss.on('connection', function(client) {
        client.pong();
      });
      var ws = new WebSocket('ws://localhost:' + port);
      ws.on('pong', function() {
        ws.terminate();
        wss.close();
        done();
      });
    });
  });

  describe('connection establishing', function() {
    it('can disconnect before connection is established', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.terminate();
        ws.on('open', function() {
          assert.fail('connect shouldnt be raised here');
        });
        ws.on('close', function() {
          srv.close();
          done();
        });
      });
    });

    it('can close before connection is established', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.close(1001);
        ws.on('open', function() {
          assert.fail('connect shouldnt be raised here');
        });
        ws.on('close', function() {
          srv.close();
          done();
        });
      });
    });

    it('can handle error before request is upgraded', function(done) {
        // Here, we don't create a server, to guarantee that the connection will
        // fail before the request is upgraded
        ++port;
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          assert.fail('connect shouldnt be raised here');
        });
        ws.on('close', function() {
          assert.fail('close shouldnt be raised here');
        });
        ws.on('error', function() {
          setTimeout(function() {
            assert.equal(ws.readyState, WebSocket.CLOSED);
            done();
          }, 50)
        });
    });

    it('invalid server key is denied', function(done) {
      server.createServer(++port, server.handlers.invalidKey, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('error', function() {
          srv.close();
          done();
        });
      });
    });

    it('close event is raised when server closes connection', function(done) {
      server.createServer(++port, server.handlers.closeAfterConnect, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('close', function() {
          srv.close();
          done();
        });
      });
    });

    it('error is emitted if server aborts connection', function(done) {
      server.createServer(++port, server.handlers.return401, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          assert.fail('connect shouldnt be raised here');
        });
        ws.on('error', function() {
          srv.close();
          done();
        });
      });
    });

    it('unexpected response can be read when sent by server', function(done) {
      server.createServer(++port, server.handlers.return401, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          assert.fail('connect shouldnt be raised here');
        });
        ws.on('unexpected-response', function(req, res) {
          assert.equal(res.statusCode, 401);

          var data = '';

          res.on('data', function (v) {
            data += v;
          });

          res.on('end', function () {
            assert.equal(data, 'Not allowed!');
            srv.close();
            done();
          });
        });
        ws.on('error', function () {
          assert.fail('error shouldnt be raised here');
        });
      });
    });

    it('request can be aborted when unexpected response is sent by server', function(done) {
      server.createServer(++port, server.handlers.return401, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          assert.fail('connect shouldnt be raised here');
        });
        ws.on('unexpected-response', function(req, res) {
          assert.equal(res.statusCode, 401);

          res.on('end', function () {
            srv.close();
            done();
          });

          req.abort();
        });
        ws.on('error', function () {
          assert.fail('error shouldnt be raised here');
        });
      });
    });
  });

  describe('#pause and #resume', function() {
    it('pauses the underlying stream', function(done) {
      // this test is sort-of racecondition'y, since an unlikely slow connection
      // to localhost can cause the test to succeed even when the stream pausing
      // isn't working as intended. that is an extremely unlikely scenario, though
      // and an acceptable risk for the test.
      var client;
      var serverClient;
      var openCount = 0;
      function onOpen() {
        if (++openCount == 2) {
          var paused = true;
          serverClient.on('message', function() {
            paused.should.not.be.ok;
            wss.close();
            done();
          });
          serverClient.pause();
          setTimeout(function() {
            paused = false;
            serverClient.resume();
          }, 200);
          client.send('foo');
        }
      }
      var wss = new WebSocketServer({port: ++port}, function() {
        var ws = new WebSocket('ws://localhost:' + port);
        serverClient = ws;
        serverClient.on('open', onOpen);
      });
      wss.on('connection', function(ws) {
        client = ws;
        onOpen();
      });
    });
  });

  describe('#ping', function() {
    it('before connect should fail', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('error', function() {});
        try {
          ws.ping();
        }
        catch (e) {
          srv.close();
          ws.terminate();
          done();
        }
      });
    });

    it('before connect can silently fail', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('error', function() {});
        ws.ping('', {}, true);
        srv.close();
        ws.terminate();
        done();
      });
    });

    it('without message is successfully transmitted to the server', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          ws.ping();
        });
        srv.on('ping', function(message) {
          srv.close();
          ws.terminate();
          done();
        });
      });
    });

    it('with message is successfully transmitted to the server', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          ws.ping('hi');
        });
        srv.on('ping', function(message) {
          assert.equal('hi', message);
          srv.close();
          ws.terminate();
          done();
        });
      });
    });

    it('can send safely receive numbers as ping payload', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);

        ws.on('open', function() {
          ws.ping(200);
        });

        srv.on('ping', function(message) {
          assert.equal('200', message);
          srv.close();
          ws.terminate();
          done();
        });
      });
    });

    it('with encoded message is successfully transmitted to the server', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          ws.ping('hi', {mask: true});
        });
        srv.on('ping', function(message, flags) {
          assert.ok(flags.masked);
          assert.equal('hi', message);
          srv.close();
          ws.terminate();
          done();
        });
      });
    });
  });

  describe('#pong', function() {
    it('before connect should fail', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('error', function() {});
        try {
          ws.pong();
        }
        catch (e) {
          srv.close();
          ws.terminate();
          done();
        }
      });
    });

    it('before connect can silently fail', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('error', function() {});
        ws.pong('', {}, true);
        srv.close();
        ws.terminate();
        done();
      });
    });

    it('without message is successfully transmitted to the server', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          ws.pong();
        });
        srv.on('pong', function(message) {
          srv.close();
          ws.terminate();
          done();
        });
      });
    });

    it('with message is successfully transmitted to the server', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          ws.pong('hi');
        });
        srv.on('pong', function(message) {
          assert.equal('hi', message);
          srv.close();
          ws.terminate();
          done();
        });
      });
    });

    it('with encoded message is successfully transmitted to the server', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          ws.pong('hi', {mask: true});
        });
        srv.on('pong', function(message, flags) {
          assert.ok(flags.masked);
          assert.equal('hi', message);
          srv.close();
          ws.terminate();
          done();
        });
      });
    });
  });

  describe('#send', function() {
    it('very long binary data can be sent and received (with echoing server)', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var array = new Float32Array(5 * 1024 * 1024);
        for (var i = 0; i < array.length; ++i) array[i] = i / 5;
        ws.on('open', function() {
          ws.send(array, {binary: true});
        });
        ws.on('message', function(message, flags) {
          assert.ok(flags.binary);
          assert.ok(areArraysEqual(array, new Float32Array(getArrayBuffer(message))));
          ws.terminate();
          srv.close();
          done();
        });
      });
    });

    it('can send and receive text data', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          ws.send('hi');
        });
        ws.on('message', function(message, flags) {
          assert.equal('hi', message);
          ws.terminate();
          srv.close();
          done();
        });
      });
    });

    it('send and receive binary data as an array', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var array = new Float32Array(6);
        for (var i = 0; i < array.length; ++i) array[i] = i / 2;
        var partial = array.subarray(2, 5);
        ws.on('open', function() {
          ws.send(partial, {binary: true});
        });
        ws.on('message', function(message, flags) {
          assert.ok(flags.binary);
          assert.ok(areArraysEqual(partial, new Float32Array(getArrayBuffer(message))));
          ws.terminate();
          srv.close();
          done();
        });
      });
    });

    it('binary data can be sent and received as buffer', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var buf = new Buffer('foobar');
        ws.on('open', function() {
          ws.send(buf, {binary: true});
        });
        ws.on('message', function(message, flags) {
          assert.ok(flags.binary);
          assert.ok(areArraysEqual(buf, message));
          ws.terminate();
          srv.close();
          done();
        });
      });
    });

    it('ArrayBuffer is auto-detected without binary flag', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var array = new Float32Array(5);
        for (var i = 0; i < array.length; ++i) array[i] = i / 2;
        ws.on('open', function() {
          ws.send(array.buffer);
        });
        ws.onmessage = function (event) {
          assert.ok(event.binary);
          assert.ok(areArraysEqual(array, new Float32Array(getArrayBuffer(event.data))));
          ws.terminate();
          srv.close();
          done();
        };
      });
    });

    it('Buffer is auto-detected without binary flag', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var buf = new Buffer('foobar');
        ws.on('open', function() {
          ws.send(buf);
        });
        ws.onmessage = function (event) {
          assert.ok(event.binary);
          assert.ok(areArraysEqual(event.data, buf));
          ws.terminate();
          srv.close();
          done();
        };
      });
    });

    it('before connect should fail', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('error', function() {});
        try {
          ws.send('hi');
        }
        catch (e) {
          ws.terminate();
          srv.close();
          done();
        }
      });
    });

    it('before connect should pass error through callback, if present', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('error', function() {});
        ws.send('hi', function(error) {
          assert.ok(error instanceof Error);
          ws.terminate();
          srv.close();
          done();
        });
      });
    });

    it('without data should be successful', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          ws.send();
        });
        srv.on('message', function(message, flags) {
          assert.equal('', message);
          srv.close();
          ws.terminate();
          done();
        });
      });
    });

    it('calls optional callback when flushed', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          ws.send('hi', function() {
            srv.close();
            ws.terminate();
            done();
          });
        });
      });
    });

    it('with unencoded message is successfully transmitted to the server', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          ws.send('hi');
        });
        srv.on('message', function(message, flags) {
          assert.equal('hi', message);
          srv.close();
          ws.terminate();
          done();
        });
      });
    });

    it('with encoded message is successfully transmitted to the server', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          ws.send('hi', {mask: true});
        });
        srv.on('message', function(message, flags) {
          assert.ok(flags.masked);
          assert.equal('hi', message);
          srv.close();
          ws.terminate();
          done();
        });
      });
    });

    it('with unencoded binary message is successfully transmitted to the server', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var array = new Float32Array(5);
        for (var i = 0; i < array.length; ++i) array[i] = i / 2;
        ws.on('open', function() {
          ws.send(array, {binary: true});
        });
        srv.on('message', function(message, flags) {
          assert.ok(flags.binary);
          assert.ok(areArraysEqual(array, new Float32Array(getArrayBuffer(message))));
          srv.close();
          ws.terminate();
          done();
        });
      });
    });

    it('with encoded binary message is successfully transmitted to the server', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var array = new Float32Array(5);
        for (var i = 0; i < array.length; ++i) array[i] = i / 2;
        ws.on('open', function() {
          ws.send(array, {mask: true, binary: true});
        });
        srv.on('message', function(message, flags) {
          assert.ok(flags.binary);
          assert.ok(flags.masked);
          assert.ok(areArraysEqual(array, new Float32Array(getArrayBuffer(message))));
          srv.close();
          ws.terminate();
          done();
        });
      });
    });

    it('with binary stream will send fragmented data', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var callbackFired = false;
        ws.on('open', function() {
          var fileStream = fs.createReadStream('test/fixtures/textfile');
          fileStream.bufferSize = 100;
          ws.send(fileStream, {binary: true}, function(error) {
            assert.equal(null, error);
            callbackFired = true;
          });
        });
        srv.on('message', function(data, flags) {
          assert.ok(flags.binary);
          assert.ok(areArraysEqual(fs.readFileSync('test/fixtures/textfile'), data));
          ws.terminate();
        });
        ws.on('close', function() {
          assert.ok(callbackFired);
          srv.close();
          done();
        });
      });
    });

    it('with text stream will send fragmented data', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var callbackFired = false;
        ws.on('open', function() {
          var fileStream = fs.createReadStream('test/fixtures/textfile');
          fileStream.setEncoding('utf8');
          fileStream.bufferSize = 100;
          ws.send(fileStream, {binary: false}, function(error) {
            assert.equal(null, error);
            callbackFired = true;
          });
        });
        srv.on('message', function(data, flags) {
          assert.ok(!flags.binary);
          assert.ok(areArraysEqual(fs.readFileSync('test/fixtures/textfile', 'utf8'), data));
          ws.terminate();
        });
        ws.on('close', function() {
          assert.ok(callbackFired);
          srv.close();
          done();
        });
      });
    });

    it('will cause intermittent send to be delayed in order', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          var fileStream = fs.createReadStream('test/fixtures/textfile');
          fileStream.setEncoding('utf8');
          fileStream.bufferSize = 100;
          ws.send(fileStream);
          ws.send('foobar');
          ws.send('baz');
        });
        var receivedIndex = 0;
        srv.on('message', function(data, flags) {
          ++receivedIndex;
          if (receivedIndex == 1) {
            assert.ok(!flags.binary);
            assert.ok(areArraysEqual(fs.readFileSync('test/fixtures/textfile', 'utf8'), data));
          }
          else if (receivedIndex == 2) {
            assert.ok(!flags.binary);
            assert.equal('foobar', data);
          }
          else {
            assert.ok(!flags.binary);
            assert.equal('baz', data);
            srv.close();
            ws.terminate();
            done();
          }
        });
      });
    });

    it('will cause intermittent stream to be delayed in order', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          var fileStream = fs.createReadStream('test/fixtures/textfile');
          fileStream.setEncoding('utf8');
          fileStream.bufferSize = 100;
          ws.send(fileStream);
          var i = 0;
          ws.stream(function(error, send) {
            assert.ok(!error);
            if (++i == 1) send('foo');
            else send('bar', true);
          });
        });
        var receivedIndex = 0;
        srv.on('message', function(data, flags) {
          ++receivedIndex;
          if (receivedIndex == 1) {
            assert.ok(!flags.binary);
            assert.ok(areArraysEqual(fs.readFileSync('test/fixtures/textfile', 'utf8'), data));
          }
          else if (receivedIndex == 2) {
            assert.ok(!flags.binary);
            assert.equal('foobar', data);
            srv.close();
            ws.terminate();
            done();
          }
        });
      });
    });

    it('will cause intermittent ping to be delivered', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          var fileStream = fs.createReadStream('test/fixtures/textfile');
          fileStream.setEncoding('utf8');
          fileStream.bufferSize = 100;
          ws.send(fileStream);
          ws.ping('foobar');
        });
        var receivedIndex = 0;
        srv.on('message', function(data, flags) {
          assert.ok(!flags.binary);
          assert.ok(areArraysEqual(fs.readFileSync('test/fixtures/textfile', 'utf8'), data));
          if (++receivedIndex == 2) {
            srv.close();
            ws.terminate();
            done();
          }
        });
        srv.on('ping', function(data) {
          assert.equal('foobar', data);
          if (++receivedIndex == 2) {
            srv.close();
            ws.terminate();
            done();
          }
        });
      });
    });

    it('will cause intermittent pong to be delivered', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          var fileStream = fs.createReadStream('test/fixtures/textfile');
          fileStream.setEncoding('utf8');
          fileStream.bufferSize = 100;
          ws.send(fileStream);
          ws.pong('foobar');
        });
        var receivedIndex = 0;
        srv.on('message', function(data, flags) {
          assert.ok(!flags.binary);
          assert.ok(areArraysEqual(fs.readFileSync('test/fixtures/textfile', 'utf8'), data));
          if (++receivedIndex == 2) {
            srv.close();
            ws.terminate();
            done();
          }
        });
        srv.on('pong', function(data) {
          assert.equal('foobar', data);
          if (++receivedIndex == 2) {
            srv.close();
            ws.terminate();
            done();
          }
        });
      });
    });

    it('will cause intermittent close to be delivered', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          var fileStream = fs.createReadStream('test/fixtures/textfile');
          fileStream.setEncoding('utf8');
          fileStream.bufferSize = 100;
          ws.send(fileStream);
          ws.close(1000, 'foobar');
        });
        ws.on('close', function() {
          srv.close();
          ws.terminate();
          done();
        });
        ws.on('error', function() { /* That's quite alright -- a send was attempted after close */ });
        srv.on('message', function(data, flags) {
          assert.ok(!flags.binary);
          assert.ok(areArraysEqual(fs.readFileSync('test/fixtures/textfile', 'utf8'), data));
        });
        srv.on('close', function(code, data) {
          assert.equal(1000, code);
          assert.equal('foobar', data);
        });
      });
    });
  });

  describe('#stream', function() {
    it('very long binary data can be streamed', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var buffer = new Buffer(10 * 1024);
        for (var i = 0; i < buffer.length; ++i) buffer[i] = i % 0xff;
        ws.on('open', function() {
          var i = 0;
          var blockSize = 800;
          var bufLen = buffer.length;
          ws.stream({binary: true}, function(error, send) {
            assert.ok(!error);
            var start = i * blockSize;
            var toSend = Math.min(blockSize, bufLen - (i * blockSize));
            var end = start + toSend;
            var isFinal = toSend < blockSize;
            send(buffer.slice(start, end), isFinal);
            i += 1;
          });
        });
        srv.on('message', function(data, flags) {
          assert.ok(flags.binary);
          assert.ok(areArraysEqual(buffer, data));
          ws.terminate();
          srv.close();
          done();
        });
      });
    });

    it('before connect should pass error through callback', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('error', function() {});
        ws.stream(function(error) {
          assert.ok(error instanceof Error);
          ws.terminate();
          srv.close();
          done();
        });
      });
    });

    it('without callback should fail', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var payload = 'HelloWorld';
        ws.on('open', function() {
          try {
            ws.stream();
          }
          catch (e) {
            srv.close();
            ws.terminate();
            done();
          }
        });
      });
    });

    it('will cause intermittent send to be delayed in order', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var payload = 'HelloWorld';
        ws.on('open', function() {
          var i = 0;
          ws.stream(function(error, send) {
            assert.ok(!error);
            if (++i == 1) {
              send(payload.substr(0, 5));
              ws.send('foobar');
              ws.send('baz');
            }
            else {
              send(payload.substr(5, 5), true);
            }
          });
        });
        var receivedIndex = 0;
        srv.on('message', function(data, flags) {
          ++receivedIndex;
          if (receivedIndex == 1) {
            assert.ok(!flags.binary);
            assert.equal(payload, data);
          }
          else if (receivedIndex == 2) {
            assert.ok(!flags.binary);
            assert.equal('foobar', data);
          }
          else {
            assert.ok(!flags.binary);
            assert.equal('baz', data);
            srv.close();
            ws.terminate();
            done();
          }
        });
      });
    });

    it('will cause intermittent stream to be delayed in order', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var payload = 'HelloWorld';
        ws.on('open', function() {
          var i = 0;
          ws.stream(function(error, send) {
            assert.ok(!error);
            if (++i == 1) {
              send(payload.substr(0, 5));
              var i2 = 0;
              ws.stream(function(error, send) {
                assert.ok(!error);
                if (++i2 == 1) send('foo');
                else send('bar', true);
              });
              ws.send('baz');
            }
            else send(payload.substr(5, 5), true);
          });
        });
        var receivedIndex = 0;
        srv.on('message', function(data, flags) {
          ++receivedIndex;
          if (receivedIndex == 1) {
            assert.ok(!flags.binary);
            assert.equal(payload, data);
          }
          else if (receivedIndex == 2) {
            assert.ok(!flags.binary);
            assert.equal('foobar', data);
          }
          else if (receivedIndex == 3){
            assert.ok(!flags.binary);
            assert.equal('baz', data);
            setTimeout(function() {
              srv.close();
              ws.terminate();
              done();
            }, 1000);
          }
          else throw new Error('more messages than we actually sent just arrived');
        });
      });
    });

    it('will cause intermittent ping to be delivered', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var payload = 'HelloWorld';
        ws.on('open', function() {
          var i = 0;
          ws.stream(function(error, send) {
            assert.ok(!error);
            if (++i == 1) {
              send(payload.substr(0, 5));
              ws.ping('foobar');
            }
            else {
              send(payload.substr(5, 5), true);
            }
          });
        });
        var receivedIndex = 0;
        srv.on('message', function(data, flags) {
          assert.ok(!flags.binary);
          assert.equal(payload, data);
          if (++receivedIndex == 2) {
            srv.close();
            ws.terminate();
            done();
          }
        });
        srv.on('ping', function(data) {
          assert.equal('foobar', data);
          if (++receivedIndex == 2) {
            srv.close();
            ws.terminate();
            done();
          }
        });
      });
    });

    it('will cause intermittent pong to be delivered', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var payload = 'HelloWorld';
        ws.on('open', function() {
          var i = 0;
          ws.stream(function(error, send) {
            assert.ok(!error);
            if (++i == 1) {
              send(payload.substr(0, 5));
              ws.pong('foobar');
            }
            else {
              send(payload.substr(5, 5), true);
            }
          });
        });
        var receivedIndex = 0;
        srv.on('message', function(data, flags) {
          assert.ok(!flags.binary);
          assert.equal(payload, data);
          if (++receivedIndex == 2) {
            srv.close();
            ws.terminate();
            done();
          }
        });
        srv.on('pong', function(data) {
          assert.equal('foobar', data);
          if (++receivedIndex == 2) {
            srv.close();
            ws.terminate();
            done();
          }
        });
      });
    });

    it('will cause intermittent close to be delivered', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var payload = 'HelloWorld';
        var errorGiven = false;
        ws.on('open', function() {
          var i = 0;
          ws.stream(function(error, send) {
            if (++i == 1) {
              send(payload.substr(0, 5));
              ws.close(1000, 'foobar');
            }
            else if(i == 2) {
              send(payload.substr(5, 5), true);
            }
            else if (i == 3) {
              assert.ok(error);
              errorGiven = true;
            }
          });
        });
        ws.on('close', function() {
          assert.ok(errorGiven);
          srv.close();
          ws.terminate();
          done();
        });
        srv.on('message', function(data, flags) {
          assert.ok(!flags.binary);
          assert.equal(payload, data);
        });
        srv.on('close', function(code, data) {
          assert.equal(1000, code);
          assert.equal('foobar', data);
        });
      });
    });
  });

  describe('#close', function() {
    it('will raise error callback, if any, if called during send stream', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var errorGiven = false;
        ws.on('open', function() {
          var fileStream = fs.createReadStream('test/fixtures/textfile');
          fileStream.setEncoding('utf8');
          fileStream.bufferSize = 100;
          ws.send(fileStream, function(error) {
            errorGiven = error != null;
          });
          ws.close(1000, 'foobar');
        });
        ws.on('close', function() {
          setTimeout(function() {
            assert.ok(errorGiven);
            srv.close();
            ws.terminate();
            done();
          }, 1000);
        });
      });
    });

    it('without invalid first argument throws exception', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          try {
            ws.close('error');
          }
          catch (e) {
            srv.close();
            ws.terminate();
            done();
          }
        });
      });
    });

    it('without reserved error code 1004 throws exception', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          try {
            ws.close(1004);
          }
          catch (e) {
            srv.close();
            ws.terminate();
            done();
          }
        });
      });
    });

    it('without message is successfully transmitted to the server', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          ws.close(1000);
        });
        srv.on('close', function(code, message, flags) {
          assert.equal('', message);
          srv.close();
          ws.terminate();
          done();
        });
      });
    });

    it('with message is successfully transmitted to the server', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          ws.close(1000, 'some reason');
        });
        srv.on('close', function(code, message, flags) {
          assert.ok(flags.masked);
          assert.equal('some reason', message);
          srv.close();
          ws.terminate();
          done();
        });
      });
    });

    it('with encoded message is successfully transmitted to the server', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('open', function() {
          ws.close(1000, 'some reason', {mask: true});
        });
        srv.on('close', function(code, message, flags) {
          assert.ok(flags.masked);
          assert.equal('some reason', message);
          srv.close();
          ws.terminate();
          done();
        });
      });
    });

    it('ends connection to the server', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var connectedOnce = false;
        ws.on('open', function() {
          connectedOnce = true;
          ws.close(1000, 'some reason', {mask: true});
        });
        ws.on('close', function() {
          assert.ok(connectedOnce);
          srv.close();
          ws.terminate();
          done();
        });
      });
    });

    it('consumes all data when the server socket closed', function(done) {
      var wss = new WebSocketServer({port: ++port}, function() {
        wss.on('connection', function(conn) {
          conn.send('foo');
          conn.send('bar');
          conn.send('baz');
          conn.close();
        });
        var ws = new WebSocket('ws://localhost:' + port);
        var messages = [];
        ws.on('message', function (message) {
          messages.push(message);
          if (messages.length === 3) {
            assert.deepEqual(messages, ['foo', 'bar', 'baz']);
            wss.close();
            ws.terminate();
            done();
          }
        });
      });
    });
  });

  describe('W3C API emulation', function() {
    it('should not throw errors when getting and setting', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var listener = function () {};

        ws.onmessage = listener;
        ws.onerror = listener;
        ws.onclose = listener;
        ws.onopen = listener;

        assert.ok(ws.onopen === listener);
        assert.ok(ws.onmessage === listener);
        assert.ok(ws.onclose === listener);
        assert.ok(ws.onerror === listener);

        srv.close();
        ws.terminate();
        done();
      });
    });

    it('should work the same as the EventEmitter api', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        var listener = function() {};
        var message = 0;
        var close = 0;
        var open = 0;

        ws.onmessage = function(messageEvent) {
          assert.ok(!!messageEvent.data);
          ++message;
          ws.close();
        };

        ws.onopen = function() {
          ++open;
        }

        ws.onclose = function() {
          ++close;
        }

        ws.on('open', function() {
          ws.send('foo');
        });

        ws.on('close', function() {
          process.nextTick(function() {
            assert.ok(message === 1);
            assert.ok(open === 1);
            assert.ok(close === 1);

            srv.close();
            ws.terminate();
            done();
          });
        });
      });
    });

    it('should receive text data wrapped in a MessageEvent when using addEventListener', function(done) {
      server.createServer(++port, function(srv) {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.addEventListener('open', function() {
          ws.send('hi');
        });
        ws.addEventListener('message', function(messageEvent) {
          assert.equal('hi', messageEvent.data);
          ws.terminate();
          srv.close();
          done();
        });
      });
    });

    it('should receive valid CloseEvent when server closes with code 1000', function(done) {
      var wss = new WebSocketServer({port: ++port}, function() {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.addEventListener('close', function(closeEvent) {
          assert.equal(true, closeEvent.wasClean);
          assert.equal(1000, closeEvent.code);
          ws.terminate();
          wss.close();
          done();
        });
      });
      wss.on('connection', function(client) {
        client.close(1000);
      });
    });

    it('should receive valid CloseEvent when server closes with code 1001', function(done) {
      var wss = new WebSocketServer({port: ++port}, function() {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.addEventListener('close', function(closeEvent) {
          assert.equal(false, closeEvent.wasClean);
          assert.equal(1001, closeEvent.code);
          assert.equal('some daft reason', closeEvent.reason);
          ws.terminate();
          wss.close();
          done();
        });
      });
      wss.on('connection', function(client) {
        client.close(1001, 'some daft reason');
      });
    });

    it('should have target set on Events', function(done) {
      var wss = new WebSocketServer({port: ++port}, function() {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.addEventListener('open', function(openEvent) {
          assert.equal(ws, openEvent.target);
        });
        ws.addEventListener('message', function(messageEvent) {
          assert.equal(ws, messageEvent.target);
          wss.close();
        });
        ws.addEventListener('close', function(closeEvent) {
          assert.equal(ws, closeEvent.target);
          ws.emit('error', new Error('forced'));
        });
        ws.addEventListener('error', function(errorEvent) {
          assert.equal(errorEvent.message, 'forced');
          assert.equal(ws, errorEvent.target);
          ws.terminate();
          done();
        });
      });
      wss.on('connection', function(client) {
        client.send('hi')
      });
    });

    it('should have type set on Events', function(done) {
      var wss = new WebSocketServer({port: ++port}, function() {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.addEventListener('open', function(openEvent) {
          assert.equal('open', openEvent.type);
        });
        ws.addEventListener('message', function(messageEvent) {
          assert.equal('message', messageEvent.type);
          wss.close();
        });
        ws.addEventListener('close', function(closeEvent) {
          assert.equal('close', closeEvent.type);
          ws.emit('error', new Error('forced'));
        });
        ws.addEventListener('error', function(errorEvent) {
          assert.equal(errorEvent.message, 'forced');
          assert.equal('error', errorEvent.type);
          ws.terminate();
          done();
        });
      });
      wss.on('connection', function(client) {
        client.send('hi')
      });
    });
  });

  describe('ssl', function() {
    it('can connect to secure websocket server', function(done) {
      var options = {
        key: fs.readFileSync('test/fixtures/key.pem'),
        cert: fs.readFileSync('test/fixtures/certificate.pem')
      };
      var app = https.createServer(options, function (req, res) {
        res.writeHead(200);
        res.end();
      });
      var wss = new WebSocketServer({server: app});
      app.listen(++port, function() {
        var ws = new WebSocket('wss://localhost:' + port);
      });
      wss.on('connection', function(ws) {
        app.close();
        ws.terminate();
        wss.close();
        done();
      });
    });

    it('can connect to secure websocket server with client side certificate', function(done) {
      var options = {
        key: fs.readFileSync('test/fixtures/key.pem'),
        cert: fs.readFileSync('test/fixtures/certificate.pem'),
        ca: [fs.readFileSync('test/fixtures/ca1-cert.pem')],
        requestCert: true
      };
      var clientOptions = {
        key: fs.readFileSync('test/fixtures/agent1-key.pem'),
        cert: fs.readFileSync('test/fixtures/agent1-cert.pem')
      };
      var app = https.createServer(options, function (req, res) {
        res.writeHead(200);
        res.end();
      });
      var success = false;
      var wss = new WebSocketServer({
        server: app,
        verifyClient: function(info) {
          success = !!info.req.client.authorized;
          return true;
        }
      });
      app.listen(++port, function() {
        var ws = new WebSocket('wss://localhost:' + port, clientOptions);
      });
      wss.on('connection', function(ws) {
        app.close();
        ws.terminate();
        wss.close();
        success.should.be.ok;
        done();
      });
    });

    it('cannot connect to secure websocket server via ws://', function(done) {
      var options = {
        key: fs.readFileSync('test/fixtures/key.pem'),
        cert: fs.readFileSync('test/fixtures/certificate.pem')
      };
      var app = https.createServer(options, function (req, res) {
        res.writeHead(200);
        res.end();
      });
      var wss = new WebSocketServer({server: app});
      app.listen(++port, function() {
        var ws = new WebSocket('ws://localhost:' + port, { rejectUnauthorized :false });
        ws.on('error', function() {
          app.close();
          ws.terminate();
          wss.close();
          done();
        });
      });
    });

    it('can send and receive text data', function(done) {
      var options = {
        key: fs.readFileSync('test/fixtures/key.pem'),
        cert: fs.readFileSync('test/fixtures/certificate.pem')
      };
      var app = https.createServer(options, function (req, res) {
        res.writeHead(200);
        res.end();
      });
      var wss = new WebSocketServer({server: app});
      app.listen(++port, function() {
        var ws = new WebSocket('wss://localhost:' + port);
        ws.on('open', function() {
          ws.send('foobar');
        });
      });
      wss.on('connection', function(ws) {
        ws.on('message', function(message, flags) {
          message.should.eql('foobar');
          app.close();
          ws.terminate();
          wss.close();
          done();
        });
      });
    });

    it('can send and receive very long binary data', function(done) {
      var options = {
        key: fs.readFileSync('test/fixtures/key.pem'),
        cert: fs.readFileSync('test/fixtures/certificate.pem')
      }
      var app = https.createServer(options, function (req, res) {
        res.writeHead(200);
        res.end();
      });
      crypto.randomBytes(5 * 1024 * 1024, function(ex, buf) {
        if (ex) throw ex;
        var wss = new WebSocketServer({server: app});
        app.listen(++port, function() {
          var ws = new WebSocket('wss://localhost:' + port);
          ws.on('open', function() {
            ws.send(buf, {binary: true});
          });
          ws.on('message', function(message, flags) {
            flags.binary.should.be.ok;
            areArraysEqual(buf, message).should.be.ok;
            app.close();
            ws.terminate();
            wss.close();
            done();
          });
        });
        wss.on('connection', function(ws) {
          ws.on('message', function(message, flags) {
            ws.send(message, {binary: true});
          });
        });
      });
    });
  });

  describe('protocol support discovery', function() {
    describe('#supports', function() {
      describe('#binary', function() {
        it('returns true for hybi transport', function(done) {
          var wss = new WebSocketServer({port: ++port}, function() {
            var ws = new WebSocket('ws://localhost:' + port);
          });
          wss.on('connection', function(client) {
            assert.equal(true, client.supports.binary);
            wss.close();
            done();
          });
        });

        it('returns false for hixie transport', function(done) {
          var wss = new WebSocketServer({port: ++port}, function() {
            var options = {
              port: port,
              host: '127.0.0.1',
              headers: {
                'Connection': 'Upgrade',
                'Upgrade': 'WebSocket',
                'Sec-WebSocket-Key1': '3e6b263  4 17 80',
                'Sec-WebSocket-Key2': '17  9 G`ZD9   2 2b 7X 3 /r90'
              }
            };
            var req = http.request(options);
            req.write('WjN}|M(6');
            req.end();
          });
          wss.on('connection', function(client) {
            assert.equal(false, client.supports.binary);
            wss.close();
            done();
          });
        });
      });
    });
  });

  describe('host and origin headers', function() {
    it('includes the host header with port number', function(done) {
      var srv = http.createServer();
      srv.listen(++port, function(){
        srv.on('upgrade', function(req, socket, upgradeHeade) {
          assert.equal('localhost:' + port, req.headers['host']);
          srv.close();
          done();
        });
        var ws = new WebSocket('ws://localhost:' + port);
      });
    });

    it('lacks default origin header', function(done) {
      var srv = http.createServer();
      srv.listen(++port, function() {
        srv.on('upgrade', function(req, socket, upgradeHeade) {
          req.headers.should.not.have.property('origin');
          srv.close();
          done();
        });
        var ws = new WebSocket('ws://localhost:' + port);
      });
    });

    it('honors origin set in options', function(done) {
      var srv = http.createServer();
      srv.listen(++port, function() {
        var options = {origin: 'https://example.com:8000'}
        srv.on('upgrade', function(req, socket, upgradeHeade) {
          assert.equal(options.origin, req.headers['origin']);
          srv.close();
          done();
        });
        var ws = new WebSocket('ws://localhost:' + port, options);
      });
    });

    it('excludes default ports from host header', function(done) {
      // can't create a server listening on ports 80 or 443
      // so we need to expose the method that does this
      var buildHostHeader = WebSocket.buildHostHeader
      var host = buildHostHeader(false, 'localhost', 80)
      assert.equal('localhost', host);
      host = buildHostHeader(false, 'localhost', 88)
      assert.equal('localhost:88', host);
      host = buildHostHeader(true, 'localhost', 443)
      assert.equal('localhost', host);
      host = buildHostHeader(true, 'localhost', 8443)
      assert.equal('localhost:8443', host);
      done()
    });
  });

  describe('permessage-deflate', function() {
    it('is enabled by default', function(done) {
      var srv = http.createServer(function (req, res) {});
      var wss = new WebSocketServer({server: srv, perMessageDeflate: true});
      srv.listen(++port, function() {
        var ws = new WebSocket('ws://localhost:' + port);
        srv.on('upgrade', function(req, socket, head) {
          // assert.ok(~req.headers['sec-websocket-extensions'].indexOf('permessage-deflate'));
        });
        ws.on('open', function() {
          // assert.ok(ws.extensions['permessage-deflate']);
          ws.terminate();
          wss.close();
          done();
        });
      });
    });

    it('can be disabled', function(done) {
      var srv = http.createServer(function (req, res) {});
      var wss = new WebSocketServer({server: srv, perMessageDeflate: true});
      srv.listen(++port, function() {
        var ws = new WebSocket('ws://localhost:' + port, {perMessageDeflate: false});
        srv.on('upgrade', function(req, socket, head) {
          assert.ok(!req.headers['sec-websocket-extensions']);
          ws.terminate();
          wss.close();
          done();
        });
      });
    });

    it('can send extension parameters', function(done) {
      var srv = http.createServer(function (req, res) {});
      var wss = new WebSocketServer({server: srv, perMessageDeflate: true});
      srv.listen(++port, function() {
        var ws = new WebSocket('ws://localhost:' + port, {
          perMessageDeflate: {
            serverNoContextTakeover: true,
            clientNoContextTakeover: true,
            serverMaxWindowBits: 10,
            clientMaxWindowBits: true
          }
        });
        srv.on('upgrade', function(req, socket, head) {
          var extensions = req.headers['sec-websocket-extensions'];
          assert.ok(~extensions.indexOf('permessage-deflate'));
          assert.ok(~extensions.indexOf('server_no_context_takeover'));
          assert.ok(~extensions.indexOf('client_no_context_takeover'));
          assert.ok(~extensions.indexOf('server_max_window_bits=10'));
          assert.ok(~extensions.indexOf('client_max_window_bits'));
          ws.terminate();
          wss.close();
          done();
        });
      });
    });

    it('can send and receive text data', function(done) {
      var wss = new WebSocketServer({port: ++port, perMessageDeflate: true}, function() {
        var ws = new WebSocket('ws://localhost:' + port, {perMessageDeflate: true});
        ws.on('open', function() {
          ws.send('hi', {compress: true});
        });
        ws.on('message', function(message, flags) {
          assert.equal('hi', message);
          ws.terminate();
          wss.close();
          done();
        });
      });
      wss.on('connection', function(ws) {
        ws.on('message', function(message, flags) {
          ws.send(message, {compress: true});
        });
      });
    });

    it('can send and receive a typed array', function(done) {
      var array = new Float32Array(5);
      for (var i = 0; i < array.length; i++) array[i] = i / 2;
      var wss = new WebSocketServer({port: ++port, perMessageDeflate: true}, function() {
        var ws = new WebSocket('ws://localhost:' + port, {perMessageDeflate: true});
        ws.on('open', function() {
          ws.send(array, {compress: true});
        });
        ws.on('message', function(message, flags) {
          assert.ok(areArraysEqual(array, new Float32Array(getArrayBuffer(message))));
          ws.terminate();
          wss.close();
          done();
        });
      });
      wss.on('connection', function(ws) {
        ws.on('message', function(message, flags) {
          ws.send(message, {compress: true});
        });
      });
    });

    it('can send and receive ArrayBuffer', function(done) {
      var array = new Float32Array(5);
      for (var i = 0; i < array.length; i++) array[i] = i / 2;
      var wss = new WebSocketServer({port: ++port, perMessageDeflate: true}, function() {
        var ws = new WebSocket('ws://localhost:' + port, {perMessageDeflate: true});
        ws.on('open', function() {
          ws.send(array.buffer, {compress: true});
        });
        ws.on('message', function(message, flags) {
          assert.ok(areArraysEqual(array, new Float32Array(getArrayBuffer(message))));
          ws.terminate();
          wss.close();
          done();
        });
      });
      wss.on('connection', function(ws) {
        ws.on('message', function(message, flags) {
          ws.send(message, {compress: true});
        });
      });
    });

    it('with binary stream will send fragmented data', function(done) {
      var wss = new WebSocketServer({port: ++port, perMessageDeflate: true}, function() {
        var ws = new WebSocket('ws://localhost:' + port, {perMessageDeflate: true});
        var callbackFired = false;
        ws.on('open', function() {
          var fileStream = fs.createReadStream('test/fixtures/textfile');
          fileStream.bufferSize = 100;
          ws.send(fileStream, {binary: true, compress: true}, function(error) {
            assert.equal(null, error);
            callbackFired = true;
          });
        });
        ws.on('close', function() {
          assert.ok(callbackFired);
          wss.close();
          done();
        });
      });
      wss.on('connection', function(ws) {
        ws.on('message', function(data, flags) {
          assert.ok(flags.binary);
          assert.ok(areArraysEqual(fs.readFileSync('test/fixtures/textfile'), data));
          ws.terminate();
        });
      });
    });

    describe('#send', function() {
      it('can set the compress option true when perMessageDeflate is disabled', function(done) {
        var wss = new WebSocketServer({port: ++port}, function() {
          var ws = new WebSocket('ws://localhost:' + port, {perMessageDeflate: false});
          ws.on('open', function() {
            ws.send('hi', {compress: true});
          });
          ws.on('message', function(message, flags) {
            assert.equal('hi', message);
            ws.terminate();
            wss.close();
            done();
          });
        });
        wss.on('connection', function(ws) {
          ws.on('message', function(message, flags) {
            ws.send(message, {compress: true});
          });
        });
      });
    });

    describe('#close', function() {
      it('should not raise error callback, if any, if called during send data', function(done) {
        var wss = new WebSocketServer({port: ++port, perMessageDeflate: true}, function() {
          var ws = new WebSocket('ws://localhost:' + port, {perMessageDeflate: true});
          var errorGiven = false;
          ws.on('open', function() {
            ws.send('hi', function(error) {
              errorGiven = error != null;
            });
            ws.close();
          });
          ws.on('close', function() {
            setTimeout(function() {
              assert.ok(!errorGiven);
              wss.close();
              ws.terminate();
              done();
            }, 1000);
          });
        });
      });
    });

    describe('#terminate', function() {
      it('will raise error callback, if any, if called during send data', function(done) {
        var wss = new WebSocketServer({port: ++port, perMessageDeflate: true}, function() {
          var ws = new WebSocket('ws://localhost:' + port, {perMessageDeflate: true});
          var errorGiven = false;
          ws.on('open', function() {
            ws.send('hi', function(error) {
              errorGiven = error != null;
            });
            ws.terminate();
          });
          ws.on('close', function() {
            setTimeout(function() {
              assert.ok(errorGiven);
              wss.close();
              ws.terminate();
              done();
            }, 1000);
          });
        });
      });

      it('can call during receiving data', function(done) {
        var wss = new WebSocketServer({port: ++port, perMessageDeflate: true}, function() {
          var ws = new WebSocket('ws://localhost:' + port, {perMessageDeflate: true});
          wss.on('connection', function(client) {
            for (var i = 0; i < 10; i++) {
              client.send('hi');
            }
            client.send('hi', function() {
              ws.terminate();
            });
          });
          ws.on('close', function() {
            setTimeout(function() {
              wss.close();
              done();
            }, 1000);
          });
        });
      });
    });
  });
});
