var http = require('http')
  , https = require('https')
  , WebSocket = require('../')
  , WebSocketServer = WebSocket.Server
  , fs = require('fs')
  , should = require('should');

var port = 8000;

function getArrayBuffer(buf) {
  var l = buf.length;
  var arrayBuf = new ArrayBuffer(l);
  for (var i = 0; i < l; ++i) {
    arrayBuf[i] = buf[i];
  }
  return arrayBuf;
}

function areArraysEqual(x, y) {
  if (x.length != y.length) return false;
  for (var i = 0, l = x.length; i < l; ++i) {
    if (x[i] !== y[i]) return false;
  }
  return true;
}

describe('WebSocketServer', function() {
  describe('#ctor', function() {
    it('should return a new instance if called without new', function(done) {
      var ws = WebSocketServer({noServer: true});
      ws.should.be.an.instanceOf(WebSocketServer);
      done();
    });
    
    it('throws an error if no option object is passed', function() {
      var gotException = false;
      try {
        var wss = new WebSocketServer();
      }
      catch (e) {
        gotException = true;
      }
      gotException.should.be.ok;
    });

    it('throws an error if no port or server is specified', function() {
      var gotException = false;
      try {
        var wss = new WebSocketServer({});
      }
      catch (e) {
        gotException = true;
      }
      gotException.should.be.ok;
    });

    it('does not throw an error if no port or server is specified, when the noServer option is true', function() {
      var gotException = false;
      try {
        var wss = new WebSocketServer({noServer: true});
      }
      catch (e) {
        gotException = true;
      }
      gotException.should.eql(false);
    });

    it('emits an error if http server bind fails', function(done) {
      var wss1 = new WebSocketServer({port: 50003});
      var wss2 = new WebSocketServer({port: 50003});
      wss2.on('error', function() {
        wss1.close();
        done();
      });
    });

    it('starts a server on a given port', function(done) {
      var wss = new WebSocketServer({port: ++port}, function() {
        var ws = new WebSocket('ws://localhost:' + port);
      });
      wss.on('connection', function(client) {
        wss.close();
        done();
      });
    });

    it('uses a precreated http server', function (done) {
      var srv = http.createServer();
      srv.listen(++port, function () {
        var wss = new WebSocketServer({server: srv});
        var ws = new WebSocket('ws://localhost:' + port);

        wss.on('connection', function(client) {
          wss.close();
          srv.close();
          done();
        });
      });
    });

    it('426s for non-Upgrade requests', function (done) {
      var wss = new WebSocketServer({ port: ++port }, function () {
        http.get('http://localhost:' + port, function (res) {
          var body = '';

          res.statusCode.should.equal(426);
          res.on('data', function (chunk) { body += chunk; });
          res.on('end', function () {
            body.should.equal(http.STATUS_CODES[426]);
            wss.close();
            done();
          });
        });
      });
    });

    // Don't test this on Windows. It throws errors for obvious reasons.
    if(!/^win/i.test(process.platform)) {
      it('uses a precreated http server listening on unix socket', function (done) {
        var srv = http.createServer();
        var sockPath = '/tmp/ws_socket_'+new Date().getTime()+'.'+Math.floor(Math.random() * 1000);
        srv.listen(sockPath, function () {
          var wss = new WebSocketServer({server: srv});
          var ws = new WebSocket('ws+unix://'+sockPath);

          wss.on('connection', function(client) {
            wss.close();
            srv.close();
            done();
          });
        });
      });
    }

    it('emits path specific connection event', function (done) {
      var srv = http.createServer();
      srv.listen(++port, function () {
        var wss = new WebSocketServer({server: srv});
        var ws = new WebSocket('ws://localhost:' + port+'/endpointName');

        wss.on('connection/endpointName', function(client) {
          wss.close();
          srv.close();
          done();
        });
      });
    });

    it('can have two different instances listening on the same http server with two different paths', function(done) {
      var srv = http.createServer();
      srv.listen(++port, function () {
        var wss1 = new WebSocketServer({server: srv, path: '/wss1'})
          , wss2 = new WebSocketServer({server: srv, path: '/wss2'});
        var doneCount = 0;
        wss1.on('connection', function(client) {
          wss1.close();
          if (++doneCount == 2) {
            srv.close();
            done();
          }
        });
        wss2.on('connection', function(client) {
          wss2.close();
          if (++doneCount == 2) {
            srv.close();
            done();
          }
        });
        var ws1 = new WebSocket('ws://localhost:' + port + '/wss1');
        var ws2 = new WebSocket('ws://localhost:' + port + '/wss2?foo=1');
      });
    });

    it('cannot have two different instances listening on the same http server with the same path', function(done) {
      var srv = http.createServer();
      srv.listen(++port, function () {
        var wss1 = new WebSocketServer({server: srv, path: '/wss1'});
        try {
          var wss2 = new WebSocketServer({server: srv, path: '/wss1'});
        }
        catch (e) {
          wss1.close();
          srv.close();
          done();
        }
      });
    });
  });

  describe('#close', function() {
    it('does not thrown when called twice', function(done) {
      var wss = new WebSocketServer({port: ++port}, function() {
        wss.close();
        wss.close();
        wss.close();

        done();
      });
    });

    it('will close all clients', function(done) {
      var wss = new WebSocketServer({port: ++port}, function() {
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('close', function() {
          if (++closes == 2) done();
        });
      });
      var closes = 0;
      wss.on('connection', function(client) {
        client.on('close', function() {
          if (++closes == 2) done();
        });
        wss.close();
      });
    });

    it('does not close a precreated server', function(done) {
      var srv = http.createServer();
      var realClose = srv.close;
      srv.close = function() {
        should.fail('must not close pre-created server');
      }
      srv.listen(++port, function () {
        var wss = new WebSocketServer({server: srv});
        var ws = new WebSocket('ws://localhost:' + port);
        wss.on('connection', function(client) {
          wss.close();
          srv.close = realClose;
          srv.close();
          done();
        });
      });
    });

    it('cleans up websocket data on a precreated server', function(done) {
      var srv = http.createServer();
      srv.listen(++port, function () {
        var wss1 = new WebSocketServer({server: srv, path: '/wss1'})
          , wss2 = new WebSocketServer({server: srv, path: '/wss2'});
        (typeof srv._webSocketPaths).should.eql('object');
        Object.keys(srv._webSocketPaths).length.should.eql(2);
        wss1.close();
        Object.keys(srv._webSocketPaths).length.should.eql(1);
        wss2.close();
        (typeof srv._webSocketPaths).should.eql('undefined');
        srv.close();
        done();
      });
    });
  });

  describe('#clients', function() {
    it('returns a list of connected clients', function(done) {
      var wss = new WebSocketServer({port: ++port}, function() {
        wss.clients.length.should.eql(0);
        var ws = new WebSocket('ws://localhost:' + port);
      });
      wss.on('connection', function(client) {
        wss.clients.length.should.eql(1);
        wss.close();
        done();
      });
    });

    it('can be disabled', function(done) {
      var wss = new WebSocketServer({port: ++port, clientTracking: false}, function() {
        wss.clients.length.should.eql(0);
        var ws = new WebSocket('ws://localhost:' + port);
      });
      wss.on('connection', function(client) {
        wss.clients.length.should.eql(0);
        wss.close();
        done();
      });
    });

    it('is updated when client terminates the connection', function(done) {
      var ws;
      var wss = new WebSocketServer({port: ++port}, function() {
        ws = new WebSocket('ws://localhost:' + port);
      });
      wss.on('connection', function(client) {
        client.on('close', function() {
          wss.clients.length.should.eql(0);
          wss.close();
          done();
        });
        ws.terminate();
      });
    });

    it('is updated when client closes the connection', function(done) {
      var ws;
      var wss = new WebSocketServer({port: ++port}, function() {
        ws = new WebSocket('ws://localhost:' + port);
      });
      wss.on('connection', function(client) {
        client.on('close', function() {
          wss.clients.length.should.eql(0);
          wss.close();
          done();
        });
        ws.close();
      });
    });
  });

  describe('#options', function() {
    it('exposes options passed to constructor', function(done) {
      var wss = new WebSocketServer({port: ++port}, function() {
        wss.options.port.should.eql(port);
        wss.close();
        done();
      });
    });
  });

  describe('#handleUpgrade', function() {
    it('can be used for a pre-existing server', function (done) {
      var srv = http.createServer();
      srv.listen(++port, function () {
        var wss = new WebSocketServer({noServer: true});
        srv.on('upgrade', function(req, socket, upgradeHead) {
          wss.handleUpgrade(req, socket, upgradeHead, function(client) {
            client.send('hello');
          });
        });
        var ws = new WebSocket('ws://localhost:' + port);
        ws.on('message', function(message) {
          message.should.eql('hello');
          wss.close();
          srv.close();
          done();
        });
      });
    });
  });

  describe('hybi mode', function() {
    describe('connection establishing', function() {
      it('does not accept connections with no sec-websocket-key', function(done) {
        var wss = new WebSocketServer({port: ++port}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket'
            }
          };
          var req = http.request(options);
          req.end();
          req.on('response', function(res) {
            res.statusCode.should.eql(400);
            wss.close();
            done();
          });
        });
        wss.on('connection', function(ws) {
          done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      });

      it('does not accept connections with no sec-websocket-version', function(done) {
        var wss = new WebSocketServer({port: ++port}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ=='
            }
          };
          var req = http.request(options);
          req.end();
          req.on('response', function(res) {
            res.statusCode.should.eql(400);
            wss.close();
            done();
          });
        });
        wss.on('connection', function(ws) {
          done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      });

      it('does not accept connections with invalid sec-websocket-version', function(done) {
        var wss = new WebSocketServer({port: ++port}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 12
            }
          };
          var req = http.request(options);
          req.end();
          req.on('response', function(res) {
            res.statusCode.should.eql(400);
            wss.close();
            done();
          });
        });
        wss.on('connection', function(ws) {
          done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      });

      it('client can be denied', function(done) {
        var wss = new WebSocketServer({port: ++port, verifyClient: function(o) {
          return false;
        }}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 8,
              'Sec-WebSocket-Origin': 'http://foobar.com'
            }
          };
          var req = http.request(options);
          req.end();
          req.on('response', function(res) {
            res.statusCode.should.eql(401);
            process.nextTick(function() {
              wss.close();
              done();
            });
          });
        });
        wss.on('connection', function(ws) {
          done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      });

      it('client can be accepted', function(done) {
        var wss = new WebSocketServer({port: ++port, verifyClient: function(o) {
          return true;
        }}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 13,
              'Origin': 'http://foobar.com'
            }
          };
          var req = http.request(options);
          req.end();
        });
        wss.on('connection', function(ws) {
          ws.terminate();
          wss.close();
          done();
        });
        wss.on('error', function() {});
      });

      it('verifyClient gets client origin', function(done) {
        var verifyClientCalled = false;
        var wss = new WebSocketServer({port: ++port, verifyClient: function(info) {
          info.origin.should.eql('http://foobarbaz.com');
          verifyClientCalled = true;
          return false;
        }}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 13,
              'Origin': 'http://foobarbaz.com'
            }
          };
          var req = http.request(options);
          req.end();
          req.on('response', function(res) {
            verifyClientCalled.should.be.ok;
            wss.close();
            done();
          });
        });
        wss.on('error', function() {});
      });

      it('verifyClient gets original request', function(done) {
        var verifyClientCalled = false;
        var wss = new WebSocketServer({port: ++port, verifyClient: function(info) {
          info.req.headers['sec-websocket-key'].should.eql('dGhlIHNhbXBsZSBub25jZQ==');
          verifyClientCalled = true;
          return false;
        }}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 13,
              'Origin': 'http://foobarbaz.com'
            }
          };
          var req = http.request(options);
          req.end();
          req.on('response', function(res) {
            verifyClientCalled.should.be.ok;
            wss.close();
            done();
          });
        });
        wss.on('error', function() {});
      });

      it('verifyClient has secure:true for ssl connections', function(done) {
        var options = {
          key: fs.readFileSync('test/fixtures/key.pem'),
          cert: fs.readFileSync('test/fixtures/certificate.pem')
        };
        var app = https.createServer(options, function (req, res) {
          res.writeHead(200);
          res.end();
        });
        var success = false;
        var wss = new WebSocketServer({
          server: app,
          verifyClient: function(info) {
            success = info.secure === true;
            return true;
          }
        });
        app.listen(++port, function() {
          var ws = new WebSocket('wss://localhost:' + port);
        });
        wss.on('connection', function(ws) {
          app.close();
          ws.terminate();
          wss.close();
          success.should.be.ok;
          done();
        });
      });

      it('verifyClient has secure:false for non-ssl connections', function(done) {
        var app = http.createServer(function (req, res) {
          res.writeHead(200);
          res.end();
        });
        var success = false;
        var wss = new WebSocketServer({
          server: app,
          verifyClient: function(info) {
            success = info.secure === false;
            return true;
          }
        });
        app.listen(++port, function() {
          var ws = new WebSocket('ws://localhost:' + port);
        });
        wss.on('connection', function(ws) {
          app.close();
          ws.terminate();
          wss.close();
          success.should.be.ok;
          done();
        });
      });

      it('client can be denied asynchronously', function(done) {
        var wss = new WebSocketServer({port: ++port, verifyClient: function(o, cb) {
          process.nextTick(function() {
            cb(false);
          });
        }}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 8,
              'Sec-WebSocket-Origin': 'http://foobar.com'
            }
          };
          var req = http.request(options);
          req.end();
          req.on('response', function(res) {
            res.statusCode.should.eql(401);
            process.nextTick(function() {
              wss.close();
              done();
            });
          });
        });
        wss.on('connection', function(ws) {
          done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      });

      it('client can be denied asynchronously with custom response code', function(done) {
        var wss = new WebSocketServer({port: ++port, verifyClient: function(o, cb) {
          process.nextTick(function() {
            cb(false, 404);
          });
        }}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 8,
              'Sec-WebSocket-Origin': 'http://foobar.com'
            }
          };
          var req = http.request(options);
          req.end();
          req.on('response', function(res) {
            res.statusCode.should.eql(404);
            process.nextTick(function() {
              wss.close();
              done();
            });
          });
        });
        wss.on('connection', function(ws) {
          done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      });

      it('client can be accepted asynchronously', function(done) {
        var wss = new WebSocketServer({port: ++port, verifyClient: function(o, cb) {
          process.nextTick(function() {
            cb(true);
          });
        }}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 13,
              'Origin': 'http://foobar.com'
            }
          };
          var req = http.request(options);
          req.end();
        });
        wss.on('connection', function(ws) {
          ws.terminate();
          wss.close();
          done();
        });
        wss.on('error', function() {});
      });

      it('handles messages passed along with the upgrade request (upgrade head)', function(done) {
        var wss = new WebSocketServer({port: ++port, verifyClient: function(o) {
          return true;
        }}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 13,
              'Origin': 'http://foobar.com'
            }
          };
          var req = http.request(options);
          req.write(new Buffer([0x81, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f], 'binary'));
          req.end();
        });
        wss.on('connection', function(ws) {
          ws.on('message', function(data) {
            data.should.eql('Hello');
            ws.terminate();
            wss.close();
            done();
          });
        });
        wss.on('error', function() {});
      });

      it('selects the first protocol by default', function(done) {
        var wss = new WebSocketServer({port: ++port}, function() {
          var ws = new WebSocket('ws://localhost:' + port, ['prot1', 'prot2']);
          ws.on('open', function(client) {
              ws.protocol.should.eql('prot1');
              wss.close();
              done();
          });
        });
      });

      it('selects the last protocol via protocol handler', function(done) {
        var wss = new WebSocketServer({port: ++port, handleProtocols: function(ps, cb) {
            cb(true, ps[ps.length-1]); }}, function() {
          var ws = new WebSocket('ws://localhost:' + port, ['prot1', 'prot2']);
          ws.on('open', function(client) {
              ws.protocol.should.eql('prot2');
              wss.close();
              done();
          });
        });
      });

      it('client detects invalid server protocol', function(done) {
        var wss = new WebSocketServer({port: ++port, handleProtocols: function(ps, cb) {
            cb(true, 'prot3'); }}, function() {
          var ws = new WebSocket('ws://localhost:' + port, ['prot1', 'prot2']);
          ws.on('open', function(client) {
              done(new Error('connection must not be established'));
          });
          ws.on('error', function() {
              done();
          });
        });
      });

      it('client detects no server protocol', function(done) {
        var wss = new WebSocketServer({port: ++port, handleProtocols: function(ps, cb) {
            cb(true); }}, function() {
          var ws = new WebSocket('ws://localhost:' + port, ['prot1', 'prot2']);
          ws.on('open', function(client) {
              done(new Error('connection must not be established'));
          });
          ws.on('error', function() {
              done();
          });
        });
      });

      it('client refuses server protocols', function(done) {
        var wss = new WebSocketServer({port: ++port, handleProtocols: function(ps, cb) {
            cb(false); }}, function() {
          var ws = new WebSocket('ws://localhost:' + port, ['prot1', 'prot2']);
          ws.on('open', function(client) {
              done(new Error('connection must not be established'));
          });
          ws.on('error', function() {
              done();
          });
        });
      });

      it('server detects unauthorized protocol handler', function(done) {
        var wss = new WebSocketServer({port: ++port, handleProtocols: function(ps, cb) {
          cb(false);
        }}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 13,
              'Sec-WebSocket-Origin': 'http://foobar.com'
            }
          };
          options.port = port;
          var req = http.request(options);
          req.end();
          req.on('response', function(res) {
            res.statusCode.should.eql(401);
            wss.close();
            done();
          });
        });
        wss.on('connection', function(ws) {
          done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      });

      it('server detects invalid protocol handler', function(done) {
        var wss = new WebSocketServer({port: ++port, handleProtocols: function(ps, cb) {
            // not calling callback is an error and shouldn't timeout
        }}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 13,
              'Sec-WebSocket-Origin': 'http://foobar.com'
            }
          };
          options.port = port;
          var req = http.request(options);
          req.end();
          req.on('response', function(res) {
            res.statusCode.should.eql(501);
            wss.close();
            done();
          });
        });
        wss.on('connection', function(ws) {
          done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      });

      it('accept connections with sec-websocket-extensions', function(done) {
        var wss = new WebSocketServer({port: ++port}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 13,
              'Sec-WebSocket-Extensions': 'permessage-foo; x=10'
            }
          };
          var req = http.request(options);
          req.end();
        });
        wss.on('connection', function(ws) {
          ws.terminate();
          wss.close();
          done();
        });
        wss.on('error', function() {});
      });
    });

    describe('messaging', function() {
      it('can send and receive data', function(done) {
        var data = new Array(65*1024);
        for (var i = 0; i < data.length; ++i) {
          data[i] = String.fromCharCode(65 + ~~(25 * Math.random()));
        }
        data = data.join('');
        var wss = new WebSocketServer({port: ++port}, function() {
          var ws = new WebSocket('ws://localhost:' + port);
          ws.on('message', function(message, flags) {
            ws.send(message);
          });
        });
        wss.on('connection', function(client) {
          client.on('message', function(message) {
            message.should.eql(data);
            wss.close();
            done();
          });
          client.send(data);
        });
      });
    });
  });

  describe('hixie mode', function() {
    it('can be disabled', function(done) {
      var wss = new WebSocketServer({port: ++port, disableHixie: true}, function() {
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
        req.on('response', function(res) {
          res.statusCode.should.eql(401);
          process.nextTick(function() {
            wss.close();
            done();
          });
        });
      });
      wss.on('connection', function(ws) {
        done(new Error('connection must not be established'));
      });
      wss.on('error', function() {});
    });

    describe('connection establishing', function() {
      it('does not accept connections with no sec-websocket-key1', function(done) {
        var wss = new WebSocketServer({port: ++port}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'WebSocket',
              'Sec-WebSocket-Key1': '3e6b263  4 17 80'
            }
          };
          var req = http.request(options);
          req.end();
          req.on('response', function(res) {
            res.statusCode.should.eql(400);
            wss.close();
            done();
          });
        });
        wss.on('connection', function(ws) {
          done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      });

      it('does not accept connections with no sec-websocket-key2', function(done) {
        var wss = new WebSocketServer({port: ++port}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'WebSocket',
              'Sec-WebSocket-Key2': '17  9 G`ZD9   2 2b 7X 3 /r90'
            }
          };
          var req = http.request(options);
          req.end();
          req.on('response', function(res) {
            res.statusCode.should.eql(400);
            wss.close();
            done();
          });
        });
        wss.on('connection', function(ws) {
          done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      });

      it('accepts connections with valid handshake', function(done) {
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
        wss.on('connection', function(ws) {
          ws.terminate();
          wss.close();
          done();
        });
        wss.on('error', function() {});
      });

      it('client can be denied', function(done) {
        var wss = new WebSocketServer({port: ++port, verifyClient: function(o) {
          return false;
        }}, function() {
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
          req.on('response', function(res) {
            res.statusCode.should.eql(401);
            process.nextTick(function() {
              wss.close();
              done();
            });
          });
        });
        wss.on('connection', function(ws) {
          done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      });

      it('client can be accepted', function(done) {
        var wss = new WebSocketServer({port: ++port, verifyClient: function(o) {
          return true;
        }}, function() {
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
        wss.on('connection', function(ws) {
          ws.terminate();
          wss.close();
          done();
        });
        wss.on('error', function() {});
      });

      it('verifyClient gets client origin', function(done) {
        var verifyClientCalled = false;
        var wss = new WebSocketServer({port: ++port, verifyClient: function(info) {
          info.origin.should.eql('http://foobarbaz.com');
          verifyClientCalled = true;
          return false;
        }}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'WebSocket',
              'Origin': 'http://foobarbaz.com',
              'Sec-WebSocket-Key1': '3e6b263  4 17 80',
              'Sec-WebSocket-Key2': '17  9 G`ZD9   2 2b 7X 3 /r90'
            }
          };
          var req = http.request(options);
          req.write('WjN}|M(6');
          req.end();
          req.on('response', function(res) {
            verifyClientCalled.should.be.ok;
            wss.close();
            done();
          });
        });
        wss.on('error', function() {});
      });

      it('verifyClient gets original request', function(done) {
        var verifyClientCalled = false;
        var wss = new WebSocketServer({port: ++port, verifyClient: function(info) {
          info.req.headers['sec-websocket-key1'].should.eql('3e6b263  4 17 80');
          verifyClientCalled = true;
          return false;
        }}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'WebSocket',
              'Origin': 'http://foobarbaz.com',
              'Sec-WebSocket-Key1': '3e6b263  4 17 80',
              'Sec-WebSocket-Key2': '17  9 G`ZD9   2 2b 7X 3 /r90'
            }
          };
          var req = http.request(options);
          req.write('WjN}|M(6');
          req.end();
          req.on('response', function(res) {
            verifyClientCalled.should.be.ok;
            wss.close();
            done();
          });
        });
        wss.on('error', function() {});
      });

      it('client can be denied asynchronously', function(done) {
        var wss = new WebSocketServer({port: ++port, verifyClient: function(o, cb) {
          cb(false);
        }}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'WebSocket',
              'Origin': 'http://foobarbaz.com',
              'Sec-WebSocket-Key1': '3e6b263  4 17 80',
              'Sec-WebSocket-Key2': '17  9 G`ZD9   2 2b 7X 3 /r90'
            }
          };
          var req = http.request(options);
          req.write('WjN}|M(6');
          req.end();
          req.on('response', function(res) {
            res.statusCode.should.eql(401);
            process.nextTick(function() {
              wss.close();
              done();
            });
          });
        });
        wss.on('connection', function(ws) {
          done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      });

      it('client can be denied asynchronously with custom response code', function(done) {
        var wss = new WebSocketServer({port: ++port, verifyClient: function(o, cb) {
          cb(false, 404, 'Not Found');
        }}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'WebSocket',
              'Origin': 'http://foobarbaz.com',
              'Sec-WebSocket-Key1': '3e6b263  4 17 80',
              'Sec-WebSocket-Key2': '17  9 G`ZD9   2 2b 7X 3 /r90'
            }
          };
          var req = http.request(options);
          req.write('WjN}|M(6');
          req.end();
          req.on('response', function(res) {
            res.statusCode.should.eql(404);
            process.nextTick(function() {
              wss.close();
              done();
            });
          });
        });
        wss.on('connection', function(ws) {
          done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      });

      it('client can be accepted asynchronously', function(done) {
        var wss = new WebSocketServer({port: ++port, verifyClient: function(o, cb) {
          cb(true);
        }}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'WebSocket',
              'Origin': 'http://foobarbaz.com',
              'Sec-WebSocket-Key1': '3e6b263  4 17 80',
              'Sec-WebSocket-Key2': '17  9 G`ZD9   2 2b 7X 3 /r90'
            }
          };
          var req = http.request(options);
          req.write('WjN}|M(6');
          req.end();
        });
        wss.on('connection', function(ws) {
          wss.close();
          done();
        });
        wss.on('error', function() {});
      });

      it('handles messages passed along with the upgrade request (upgrade head)', function(done) {
        var wss = new WebSocketServer({port: ++port, verifyClient: function(o) {
          return true;
        }}, function() {
          var options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'WebSocket',
              'Sec-WebSocket-Key1': '3e6b263  4 17 80',
              'Sec-WebSocket-Key2': '17  9 G`ZD9   2 2b 7X 3 /r90',
              'Origin': 'http://foobar.com'
            }
          };
          var req = http.request(options);
          req.write('WjN}|M(6');
          req.write(new Buffer([0x00, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0xff], 'binary'));
          req.end();
        });
        wss.on('connection', function(ws) {
          ws.on('message', function(data) {
            data.should.eql('Hello');
            ws.terminate();
            wss.close();
            done();
          });
        });
        wss.on('error', function() {});
      });
    });
  });

  describe('client properties', function() {
    it('protocol is exposed', function(done) {
      var wss = new WebSocketServer({port: ++port}, function() {
        var ws = new WebSocket('ws://localhost:' + port, 'hi');
      });
      wss.on('connection', function(client) {
        client.protocol.should.eql('hi');
        wss.close();
        done();
      });
    });

    it('protocolVersion is exposed', function(done) {
      var wss = new WebSocketServer({port: ++port}, function() {
        var ws = new WebSocket('ws://localhost:' + port, {protocolVersion: 8});
      });
      wss.on('connection', function(client) {
        client.protocolVersion.should.eql(8);
        wss.close();
        done();
      });
    });

    it('upgradeReq is the original request object', function(done) {
      var wss = new WebSocketServer({port: ++port}, function() {
        var ws = new WebSocket('ws://localhost:' + port, {protocolVersion: 8});
      });
      wss.on('connection', function(client) {
        client.upgradeReq.httpVersion.should.eql('1.1');
        wss.close();
        done();
      });
    });
  });

  describe('permessage-deflate', function() {
    it('accept connections with permessage-deflate extension', function(done) {
      var wss = new WebSocketServer({port: ++port}, function() {
        var options = {
          port: port,
          host: '127.0.0.1',
          headers: {
            'Connection': 'Upgrade',
            'Upgrade': 'websocket',
            'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
            'Sec-WebSocket-Version': 13,
            'Sec-WebSocket-Extensions': 'permessage-deflate; client_max_window_bits=8; server_max_window_bits=8; client_no_context_takeover; server_no_context_takeover'
          }
        };
        var req = http.request(options);
        req.end();
      });
      wss.on('connection', function(ws) {
        ws.terminate();
        wss.close();
        done();
      });
      wss.on('error', function() {});
    });

    it('does not accept connections with not defined extension parameter', function(done) {
      var wss = new WebSocketServer({port: ++port}, function() {
        var options = {
          port: port,
          host: '127.0.0.1',
          headers: {
            'Connection': 'Upgrade',
            'Upgrade': 'websocket',
            'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
            'Sec-WebSocket-Version': 13,
            'Sec-WebSocket-Extensions': 'permessage-deflate; foo=15'
          }
        };
        var req = http.request(options);
        req.end();
        req.on('response', function(res) {
          res.statusCode.should.eql(400);
          wss.close();
          done();
        });
      });
      wss.on('connection', function(ws) {
        done(new Error('connection must not be established'));
      });
      wss.on('error', function() {});
    });

    it('does not accept connections with invalid extension parameter', function(done) {
      var wss = new WebSocketServer({port: ++port}, function() {
        var options = {
          port: port,
          host: '127.0.0.1',
          headers: {
            'Connection': 'Upgrade',
            'Upgrade': 'websocket',
            'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
            'Sec-WebSocket-Version': 13,
            'Sec-WebSocket-Extensions': 'permessage-deflate; server_max_window_bits=foo'
          }
        };
        var req = http.request(options);
        req.end();
        req.on('response', function(res) {
          res.statusCode.should.eql(400);
          wss.close();
          done();
        });
      });
      wss.on('connection', function(ws) {
        done(new Error('connection must not be established'));
      });
      wss.on('error', function() {});
    });
  });
});
