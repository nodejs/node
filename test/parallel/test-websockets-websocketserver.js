'use strict';
const http = require('http');
const https = require('https');
const WebSocket = require('../../lib/websockets');
const WebSocketServer = require('../../lib/websockets').Server;
const fs = require('fs');
const assert = require('assert');

var port = 40000;

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

/*'WebSocketServer'*/
{
  /*'#ctor'*/
  {
    /*'should return a new instance if called without new'*/
    {
      var ws = WebSocketServer({noServer: true});
      assert.ok(ws instanceof WebSocketServer);
    }

    /*'throws an error if no option object is passed'*/
    {
      var gotException = false;
      try {
        let wss = new WebSocketServer();
      }
      catch (e) {
        gotException = true;
      }
      assert.ok(gotException);
    }

    /*'throws an error if no port or server is specified'*/
    {
      var gotException = false;
      try {
        let wss = new WebSocketServer({});
      }
      catch (e) {
        gotException = true;
      }
      assert.ok(gotException);
    }

    /*'does not throw an error if no port or server is specified, when the noServer option is true'*/
    {
      var gotException = false;
      try {
        let wss = new WebSocketServer({noServer: true});
      }
      catch (e) {
        gotException = true;
      }
      assert.equal(gotException, false);
    }

    /*'emits an error if http server bind fails'*/
    {
      var wss1 = new WebSocketServer({port: 50003});
      var wss2 = new WebSocketServer({port: 50003});
      wss2.on('error', function() {
        wss1.close();
      });
    }

    /*'starts a server on a given port'*/
    {
      let p = ++port
      let wss = new WebSocketServer({port: p}, function() {
        let ws = new WebSocket('ws://localhost:' + p);
      });
      wss.on('connection', function(client) {
        wss.close();
      });
    }

    /*'uses a precreated http server', function*/
    {
      let srv = http.createServer();
      let p = ++port
      srv.listen(p, function () {
        let wss = new WebSocketServer({server: srv});
        let ws = new WebSocket('ws://localhost:' + p);

        wss.on('connection', function(client) {
          wss.close();
          srv.close();
        });
      });
    }

    /*'426s for non-Upgrade requests'*/
    {
      let p = ++port
      let wss = new WebSocketServer({ port: p }, function () {
        http.get('http://localhost:' + p, function (res) {
          var body = '';

          assert.equal(res.statusCode, 426);
          res.on('data', function (chunk) { body += chunk; });
          res.on('end', function () {
            assert.equal(body, http.STATUS_CODES[426]);
            wss.close();
          });
        });
      });
    }

    // Don't test this on Windows. It throws errors for obvious reasons.
    if(!/^win/i.test(process.platform)) {
      /*'uses a precreated http server listening on unix socket'*/
      {
        let srv = http.createServer();
        let p = ++port
        var sockPath = '/tmp/ws_socket_'+ new Date().getTime() + '.' + Math.floor(Math.random() * 1000);
        srv.listen(sockPath, function () {
          let wss = new WebSocketServer({server: srv});
          var ws = new WebSocket('ws+unix://' + sockPath);

          wss.on('connection', function(client) {
            wss.close();
            srv.close();
          });
        });
      }
    }

    /*'emits path specific connection event'*/
    {
      let srv = http.createServer();
      let p = ++port
      srv.listen(p, function () {
        let wss = new WebSocketServer({server: srv});
        var ws = new WebSocket('ws://localhost:' + p +'/endpointName');

        wss.on('connection/endpointName', function(client) {
          wss.close();
          srv.close();
        });
      });
    }

    /*'can have two different instances listening on the same http server with two different paths'*/
    {
      let srv = http.createServer();
      let p = ++port
      srv.listen(p, function () {
        var wss1 = new WebSocketServer({server: srv, path: '/wss1'})
        , wss2 = new WebSocketServer({server: srv, path: '/wss2'});
        var doneCount = 0;
        wss1.on('connection', function(client) {
          wss1.close();
          if (++doneCount == 2) {
            srv.close();
          }
        });
        wss2.on('connection', function(client) {
          wss2.close();
          if (++doneCount == 2) {
            srv.close();
          }
        });
        var ws1 = new WebSocket('ws://localhost:' + p + '/wss1');
        var ws2 = new WebSocket('ws://localhost:' + p + '/wss2?foo=1');
      });
    }

    /*'cannot have two different instances listening on the same http server with the same path'*/
    {
      let srv = http.createServer();
      let p = ++port
      srv.listen(p, function () {
        var wss1 = new WebSocketServer({server: srv, path: '/wss1'});
        try {
          var wss2 = new WebSocketServer({server: srv, path: '/wss1'});
        }
        catch (e) {
          wss1.close();
          srv.close();
        }
      });
    }
  }
  //
  // /*'#close'*/
  // {
  //   /*'does not thrown when called twice'*/
  //   {
  //     let wss = new WebSocketServer({port: ++port}, function() {
  //       wss.close();
  //       wss.close();
  //       wss.close();
  //
  //       done();
  //     });
  //   }
  //
  //   /*'will close all clients'*/
  //   {
  //     let wss = new WebSocketServer({port: ++port}, function() {
  //       var ws = new WebSocket('ws://localhost:' + port);
  //       ws.on('close', function() {
  //         if (++closes == 2) done();
  //       });
  //     });
  //     var closes = 0;
  //     wss.on('connection', function(client) {
  //       client.on('close', function() {
  //         if (++closes == 2) done();
  //       });
  //       wss.close();
  //     });
  //   }
  //
  //   /*'does not close a precreated server'*/
  //   {
  //     let srv = http.createServer();
  //     var realClose = srv.close;
  //     srv.close = function() {
  //       assert.fail('must not close pre-created server');
  //     }
  //     srv.listen(++port, function () {
  //       let wss = new WebSocketServer({server: srv});
  //       var ws = new WebSocket('ws://localhost:' + port);
  //       wss.on('connection', function(client) {
  //         wss.close();
  //         srv.close = realClose;
  //         srv.close();
  //         done();
  //       });
  //     });
  //   }
  //
  //   /*'cleans up websocket data on a precreated server'*/
  //   {
  //     let srv = http.createServer();
  //     srv.listen(++port, function () {
  //       var wss1 = new WebSocketServer({server: srv, path: '/wss1'})
  //         , wss2 = new WebSocketServer({server: srv, path: '/wss2'});
  //       assert.equal((typeof srv._webSocketPaths), 'object');
  //       assert.equal(Object.keys(srv._webSocketPaths).length, 2);
  //       wss1.close();
  //       assert.equal(Object.keys(srv._webSocketPaths).length, 1);
  //       wss2.close();
  //       assert.equal((typeof srv._webSocketPaths), 'undefined');
  //       srv.close();
  //       done();
  //     });
  //   }
  // }
  //
  // /*'#clients'*/
  // {
  //   /*'returns a list of connected clients'*/
  //   {
  //     let wss = new WebSocketServer({port: ++port}, function() {
  //       assert.equal(wss.clients.length, 0);
  //       var ws = new WebSocket('ws://localhost:' + port);
  //     });
  //     wss.on('connection', function(client) {
  //       assert.equal(wss.clients.length, 1);
  //       wss.close();
  //       done();
  //     });
  //   }
  //
  //   // TODO(eljefedelrodeodeljefe): this is failing due to unknown reason
  //   /*it('can be disabled'*/
  //   {
  //   //   let wss = new WebSocketServer({port: ++port, clientTracking: false}, function() {
  //   //     assert.equal(wss.clients.length, 0);
  //   //     var ws = new WebSocket('ws://localhost:' + port);
  //   //   });
  //   //   wss.on('connection', function(client) {
  //   //     assert.equal(wss.clients.length, 0);
  //   //     wss.close();
  //   //     done();
  //   //   });
  //   // });
  //
  //   /*'is updated when client terminates the connection'*/
  //   {
  //     var ws;
  //     let wss = new WebSocketServer({port: ++port}, function() {
  //       ws = new WebSocket('ws://localhost:' + port);
  //     });
  //     wss.on('connection', function(client) {
  //       client.on('close', function() {
  //         assert.equal(wss.clients.length, 0);
  //         wss.close();
  //         done();
  //       });
  //       ws.terminate();
  //     });
  //   }
  //
  //   /*'is updated when client closes the connection'*/
  //   {
  //     var ws;
  //     let wss = new WebSocketServer({port: ++port}, function() {
  //       ws = new WebSocket('ws://localhost:' + port);
  //     });
  //     wss.on('connection', function(client) {
  //       client.on('close', function() {
  //         assert.equal(wss.clients.length, 0);
  //         wss.close();
  //         done();
  //       });
  //       ws.close();
  //     });
  //   }
  // }
  //
  /*'#options'*/
  {
    /*'exposes options passed to constructor'*/
    {
      let p = ++port
      let wss = new WebSocketServer({port: p}, function() {
        assert.equal(wss.options.port, p);
        wss.close();
      });
    }
  }

  /*'#handleUpgrade'*/
  {
    /*'can be used for a pre-existing server'*/
    {
      let srv = http.createServer();
      let p = ++port
      srv.listen(p, function () {
        let wss = new WebSocketServer({noServer: true});
        srv.on('upgrade', function(req, socket, upgradeHead) {
          wss.handleUpgrade(req, socket, upgradeHead, function(client) {
            client.send('hello');
          });
        });
        var ws = new WebSocket('ws://localhost:' + p);
        ws.on('message', function(message) {
          assert.equal(message, 'hello');
          wss.close();
          srv.close();
        });
      });
    }
  }

  /*'hybi mode'*/
  {
    /*'connection establishing'*/
    {
      /*'does not accept connections with no sec-websocket-key'*/
      {
        let p = ++port
        let wss = new WebSocketServer({port: p}, function() {
          let options = {
            port: p,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket'
            }
          };
          let req = http.request(options);
          req.end();
          req.on('response', function(res) {
            assert.equal(res.statusCode, 400);
            wss.close();
          });
        });
        wss.on('connection', function(ws) {
          // done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      }

      /*'does not accept connections with no sec-websocket-version'*/
      {
        let p = ++port
        let wss = new WebSocketServer({port: p}, function() {
          let options = {
            port: p,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ=='
            }
          };
          let req = http.request(options);
          req.end();
          req.on('response', function(res) {
            assert.equal(res.statusCode, 400);
            wss.close();
          });
        });
        wss.on('connection', function(ws) {
          // done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      }

      /*'does not accept connections with invalid sec-websocket-version'*/
      {
        let p = ++port
        let wss = new WebSocketServer({port: p}, function() {
          let options = {
            port: p,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 12
            }
          };
          let req = http.request(options);
          req.end();
          req.on('response', function(res) {
            assert.equal(res.statusCode, 400);
            wss.close();
          });
        });
        wss.on('connection', function(ws) {
          // done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      }

      /*'client can be denied'*/
      {
        let p = ++port
        let wss = new WebSocketServer({port: p, verifyClient: function(o) {
          return false;
        }}, function() {
          let options = {
            port: p,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 8,
              'Sec-WebSocket-Origin': 'http://foobar.com'
            }
          };
          let req = http.request(options);
          req.end();
          req.on('response', function(res) {
            assert.equal(res.statusCode, 401);
            process.nextTick(function() {
              wss.close();
            });
          });
        });
        wss.on('connection', function(ws) {
          // done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      }

      /*'client can be accepted'*/
      {
        let p = ++port
        let wss = new WebSocketServer({port: p, verifyClient: function(o) {
          return true;
        }}, function() {
          let options = {
            port: p,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 13,
              'Origin': 'http://foobar.com'
            }
          };
          let req = http.request(options);
          req.end();
        });
        wss.on('connection', function(ws) {
          ws.terminate();
          wss.close();
        });
        wss.on('error', function() {});
      }

      /*'verifyClient gets client origin'*/
      {
        let p = ++port
        let verifyClientCalled = false;
        let wss = new WebSocketServer({port: p, verifyClient: function(info) {
          assert.equal(info.origin, 'http://foobarbaz.com');
          verifyClientCalled = true;
          return false;
        }}, function() {
          let options = {
            port: p,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 13,
              'Origin': 'http://foobarbaz.com'
            }
          };
          let req = http.request(options);
          req.end();
          req.on('response', function(res) {
            assert.ok(verifyClientCalled);
            wss.close();
          });
        });
        wss.on('error', function() {});
      }

      /*'verifyClient gets original request'*/
      {
        let p = ++port
        let verifyClientCalled = false;
        let wss = new WebSocketServer({port: p, verifyClient: function(info) {
          assert.equal(info.req.headers['sec-websocket-key'], 'dGhlIHNhbXBsZSBub25jZQ==');
          verifyClientCalled = true;
          return false;
        }}, function() {
          let options = {
            port: p,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 13,
              'Origin': 'http://foobarbaz.com'
            }
          };
          let req = http.request(options);
          req.end();
          req.on('response', function(res) {
            assert.ok(verifyClientCalled);
            wss.close();
          });
        });
        wss.on('error', function() {});
      }

      /*'verifyClient has secure:true for ssl connections'*/
      {
        let options = {
          key: fs.readFileSync('test/fixtures/websockets/key.pem'),
          cert: fs.readFileSync('test/fixtures/websockets/certificate.pem')
        };
        var app = https.createServer(options, function (req, res) {
          res.writeHead(200);
          res.end();
        });
        var success = false;
        let wss = new WebSocketServer({
          server: app,
          verifyClient: function(info) {
            success = info.secure === true;
            return true;
          }
        });
        let p = ++port
        app.listen(p, function() {
          var ws = new WebSocket('wss://localhost:' + p);
        });
        wss.on('connection', function(ws) {
          app.close();
          ws.terminate();
          wss.close();
          assert.ok(success);
        });
      }

      /*'verifyClient has secure:false for non-ssl connections'*/
      // {
      //   var app = http.createServer(function (req, res) {
      //     res.writeHead(200);
      //     res.end();
      //   });
      //   var success = false;
      //   let wss = new WebSocketServer({
      //     server: app,
      //     verifyClient: function(info) {
      //       success = info.secure === false;
      //       return true;
      //     }
      //   });
      //   let p = ++port
      //   app.listen(p, function() {
      //     var ws = new WebSocket('ws://localhost:' + p);
      //   });
      //   wss.on('connection', function(ws) {
      //     app.close();
      //     ws.terminate();
      //     wss.close();
      //     assert.ok(success);
      //   });
      // }

      /*'client can be denied asynchronously'*/
      {
        let p = ++port
        let wss = new WebSocketServer({port: p, verifyClient: function(o, cb) {
          process.nextTick(function() {
            cb(false);
          });
        }}, function() {
          let options = {
            port: p,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 8,
              'Sec-WebSocket-Origin': 'http://foobar.com'
            }
          };
          let req = http.request(options);
          req.end();
          req.on('response', function(res) {
            assert.equal(res.statusCode, 401);
            process.nextTick(function() {
              wss.close();
            });
          });
        });
        wss.on('connection', function(ws) {
          // done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      }

      /*'client can be denied asynchronously with custom response code'*/
      {
        let p = ++port
        let wss = new WebSocketServer({port: p, verifyClient: function(o, cb) {
          process.nextTick(function() {
            cb(false, 404);
          });
        }}, function() {
          let options = {
            port: p,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 8,
              'Sec-WebSocket-Origin': 'http://foobar.com'
            }
          };
          let req = http.request(options);
          req.end();
          req.on('response', function(res) {
            assert.equal(res.statusCode, 404);
            process.nextTick(function() {
              wss.close();
            });
          });
        });
        wss.on('connection', function(ws) {
          // done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      }

      /*'client can be accepted asynchronously'*/
      {
        let p = ++port
        let wss = new WebSocketServer({port: p, verifyClient: function(o, cb) {
          process.nextTick(function() {
            cb(true);
          });
        }}, function() {
          let options = {
            port: p,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 13,
              'Origin': 'http://foobar.com'
            }
          };
          let req = http.request(options);
          req.end();
        });
        wss.on('connection', function(ws) {
          ws.terminate();
          wss.close();
        });
        wss.on('error', function() {});
      }

      /*'handles messages passed along with the upgrade request (upgrade head)'*/
      {
        let p = ++port
        let wss = new WebSocketServer({port: p, verifyClient: function(o) {
          return true;
        }}, function() {
          let options = {
            port: p,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'websocket',
              'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
              'Sec-WebSocket-Version': 13,
              'Origin': 'http://foobar.com'
            }
          };
          let req = http.request(options);
          req.write(new Buffer([0x81, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f], 'binary'));
          req.end();
        });
        wss.on('connection', function(ws) {
          ws.on('message', function(data) {
            assert.equal(data, 'Hello');
            ws.terminate();
            wss.close();
          });
        });
        wss.on('error', function() {});
      }

      /*'selects the first protocol by default'*/
      {
        let p = ++port
        let wss = new WebSocketServer({port: p}, function() {
          let ws = new WebSocket('ws://localhost:' + p, ['prot1', 'prot2']);
          ws.on('open', function(client) {
            assert.equal(ws.protocol, 'prot1');
            wss.close();
          });
        });
      }

      /*'selects the last protocol via protocol handler'*/
      {
        let p = ++port
        let wss = new WebSocketServer({port: p, handleProtocols: function(ps, cb) {
          cb(true, ps[ps.length-1]); }}, function() {
            let ws = new WebSocket('ws://localhost:' + p, ['prot1', 'prot2']);
            ws.on('open', function(client) {
              assert.equal(ws.protocol, 'prot2');
              wss.close();
            });
          });
        }

        /*'client detects invalid server protocol'*/
        // {
        //   let p = ++port
        //   let wss = new WebSocketServer({port: p, handleProtocols: function(ps, cb) {
        //       cb(true, 'prot3'); }}, function() {
        //     var ws = new WebSocket('ws://localhost:' + p, ['prot1', 'prot2']);
        //     ws.on('open', function(client) {
        //         // done(new Error('connection must not be established'));
        //     });
        //     ws.on('error', function() {});
        //   });
        // }

        /*'client detects no server protocol'*/
        // {
        //   let p = ++port
        //   let wss = new WebSocketServer({port: p, handleProtocols: function(ps, cb) {
        //       cb(true); }}, function() {
        //     var ws = new WebSocket('ws://localhost:' + p, ['prot1', 'prot2']);
        //     ws.on('open', function(client) {
        //         // done(new Error('connection must not be established'));
        //     });
        //     ws.on('error', function() {});
        //   });
        // }

        /*'client refuses server protocols'*/
        // {
        //   let p = ++port
        //   let wss = new WebSocketServer({port: p, handleProtocols: function(ps, cb) {
        //       cb(false); }}, function() {
        //     var ws = new WebSocket('ws://localhost:' + p, ['prot1', 'prot2']);
        //     ws.on('open', function(client) {
        //         // done(new Error('connection must not be established'));
        //     });
        //     ws.on('error', function() {});
        //   });
        // }

        /*'server detects unauthorized protocol handler'*/
        {
          let p = ++port
          let wss = new WebSocketServer({port: p, handleProtocols: function(ps, cb) {
            cb(false);
          }}, function() {
            let options = {
              port: p,
              host: '127.0.0.1',
              headers: {
                'Connection': 'Upgrade',
                'Upgrade': 'websocket',
                'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
                'Sec-WebSocket-Version': 13,
                'Sec-WebSocket-Origin': 'http://foobar.com'
              }
            };
            options.port = p;
            let req = http.request(options);
            req.end();
            req.on('response', function(res) {
              assert.equal(res.statusCode, 401);
              wss.close();
            });
          });
          wss.on('connection', function(ws) {
            // done(new Error('connection must not be established'));
          });
          wss.on('error', function() {});
        }

        /*'server detects invalid protocol handler'*/
        {
          let p = ++port
          let wss = new WebSocketServer({port: p, handleProtocols: function(ps, cb) {
            // not calling callback is an error and shouldn't timeout
          }}, function() {
            let options = {
              port: p,
              host: '127.0.0.1',
              headers: {
                'Connection': 'Upgrade',
                'Upgrade': 'websocket',
                'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
                'Sec-WebSocket-Version': 13,
                'Sec-WebSocket-Origin': 'http://foobar.com'
              }
            };
            options.port = p;
            let req = http.request(options);
            req.end();
            req.on('response', function(res) {
              assert.equal(res.statusCode, 501);
              wss.close();
            });
          });
          wss.on('connection', function(ws) {
            // done(new Error('connection must not be established'));
          });
          wss.on('error', function() {});
        }

        /*'accept connections with sec-websocket-extensions'*/
        {
          let p = ++port
          let wss = new WebSocketServer({port: p}, function() {
            let options = {
              port: p,
              host: '127.0.0.1',
              headers: {
                'Connection': 'Upgrade',
                'Upgrade': 'websocket',
                'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
                'Sec-WebSocket-Version': 13,
                'Sec-WebSocket-Extensions': 'permessage-foo; x=10'
              }
            };
            let req = http.request(options);
            req.end();
          });
          wss.on('connection', function(ws) {
            ws.terminate();
            wss.close();
          });
          wss.on('error', function() {});
        }
      }

      /*'messaging'*/
      {
        /*'can send and receive data'*/
        {
          let data = new Array(65*1024);
          for (var i = 0; i < data.length; ++i) {
            data[i] = String.fromCharCode(65 + ~~(25 * Math.random()));
          }
          data = data.join('');
          let p = ++port
          let wss = new WebSocketServer({port: p}, function() {
            let ws = new WebSocket('ws://localhost:' + p);
            ws.on('message', function(message, flags) {
              ws.send(message);
            });
          });
          wss.on('connection', function(client) {
            client.on('message', function(message) {
              assert.equal(message, data);
              wss.close();
            });
            client.send(data);
          });
        }
      }
    }

    /*'hixie mode'*/
    {
      /*'can be disabled'*/
      {
        let wss = new WebSocketServer({port: ++port, disableHixie: true}, function() {
          let options = {
            port: port,
            host: '127.0.0.1',
            headers: {
              'Connection': 'Upgrade',
              'Upgrade': 'WebSocket',
              'Sec-WebSocket-Key1': '3e6b263  4 17 80',
              'Sec-WebSocket-Key2': '17  9 G`ZD9   2 2b 7X 3 /r90'
            }
          };
          let req = http.request(options);
          req.write('WjN}|M(6');
          req.end();
          req.on('response', function(res) {
            assert.equal(res.statusCode, 401);
            process.nextTick(function() {
              wss.close();
            });
          });
        });
        wss.on('connection', function(ws) {
          // done(new Error('connection must not be established'));
        });
        wss.on('error', function() {});
      }

      /*'connection establishing'*/
      {
        // /*'does not accept connections with no sec-websocket-key1'*/
        // {
        //   let p = ++port
        //   let wss = new WebSocketServer({port: p}, function() {
        //     let options = {
        //       port: p,
        //       host: '127.0.0.1',
        //       headers: {
        //         'Connection': 'Upgrade',
        //         'Upgrade': 'WebSocket',
        //         'Sec-WebSocket-Key1': '3e6b263  4 17 80'
        //       }
        //     };
        //     let req = http.request(options);
        //     req.end();
        //     req.on('response', function(res) {
        //       assert.equal(res.statusCode, 400);
        //       wss.close();
        //     });
        //   });
        //   wss.on('connection', function(ws) {
        //     // done(new Error('connection must not be established'));
        //   });
        //   wss.on('error', function() {});
        // }

        /*'does not accept connections with no sec-websocket-key2'*/
        // {
        //   let p = ++port
        //   let wss = new WebSocketServer({port: p}, function() {
        //     let options = {
        //       port: p,
        //       host: '127.0.0.1',
        //       headers: {
        //         'Connection': 'Upgrade',
        //         'Upgrade': 'WebSocket',
        //         'Sec-WebSocket-Key2': '17  9 G`ZD9   2 2b 7X 3 /r90'
        //       }
        //     };
        //     let req = http.request(options);
        //     req.end();
        //     req.on('response', function(res) {
        //       assert.equal(res.statusCode, 400);
        //       wss.close();
        //     });
        //   });
        //   wss.on('connection', function(ws) {
        //     // done(new Error('connection must not be established'));
        //   });
        //   wss.on('error', function() {});
        // }

        /*'accepts connections with valid handshake'*/
        // {
        //   let p = ++port
        //   let wss = new WebSocketServer({port: p}, function() {
        //     let options = {
        //       port: p,
        //       host: '127.0.0.1',
        //       headers: {
        //         'Connection': 'Upgrade',
        //         'Upgrade': 'WebSocket',
        //         'Sec-WebSocket-Key1': '3e6b263  4 17 80',
        //         'Sec-WebSocket-Key2': '17  9 G`ZD9   2 2b 7X 3 /r90'
        //       }
        //     };
        //     let req = http.request(options);
        //     req.write('WjN}|M(6');
        //     req.end();
        //   });
        //   wss.on('connection', function(ws) {
        //     ws.terminate();
        //     wss.close();
        //   });
        //   wss.on('error', function() {});
        // }

        /*'client can be denied'*/
        {
          let p = ++port
          let wss = new WebSocketServer({port: p, verifyClient: function(o) {
            return false;
          }}, function() {
            let options = {
              port: p,
              host: '127.0.0.1',
              headers: {
                'Connection': 'Upgrade',
                'Upgrade': 'WebSocket',
                'Sec-WebSocket-Key1': '3e6b263  4 17 80',
                'Sec-WebSocket-Key2': '17  9 G`ZD9   2 2b 7X 3 /r90'
              }
            };
            let req = http.request(options);
            req.write('WjN}|M(6');
            req.end();
            req.on('response', function(res) {
              assert.equal(res.statusCode, 401);
              process.nextTick(function() {
                wss.close();
              });
            });
          });
          wss.on('connection', function(ws) {
            // done(new Error('connection must not be established'));
          });
          wss.on('error', function() {});
        }

        /*'client can be accepted'*/
        // {
        //   let p = ++port
        //   let wss = new WebSocketServer({port: p, verifyClient: function(o) {
        //     return true;
        //   }}, function() {
        //     let options = {
        //       port: p,
        //       host: '127.0.0.1',
        //       headers: {
        //         'Connection': 'Upgrade',
        //         'Upgrade': 'WebSocket',
        //         'Sec-WebSocket-Key1': '3e6b263  4 17 80',
        //         'Sec-WebSocket-Key2': '17  9 G`ZD9   2 2b 7X 3 /r90'
        //       }
        //     };
        //     let req = http.request(options);
        //     req.write('WjN}|M(6');
        //     req.end();
        //   });
        //   wss.on('connection', function(ws) {
        //     ws.terminate();
        //     wss.close();
        //   });
        //   wss.on('error', function() {});
        // }

        /*'verifyClient gets client origin'*/
        // {
        //   let p = ++port
        //   var verifyClientCalled = false;
        //   let wss = new WebSocketServer({port: p, verifyClient: function(info) {
        //     assert.equal(info.origin, 'http://foobarbaz.com');
        //     verifyClientCalled = true;
        //     return false;
        //   }}, function() {
        //     let options = {
        //       port: p,
        //       host: '127.0.0.1',
        //       headers: {
        //         'Connection': 'Upgrade',
        //         'Upgrade': 'WebSocket',
        //         'Origin': 'http://foobarbaz.com',
        //         'Sec-WebSocket-Key1': '3e6b263  4 17 80',
        //         'Sec-WebSocket-Key2': '17  9 G`ZD9   2 2b 7X 3 /r90'
        //       }
        //     };
        //     let req = http.request(options);
        //     req.write('WjN}|M(6');
        //     req.end();
        //     req.on('response', function(res) {
        //       assert.ok(verifyClientCalled);
        //       wss.close();
        //     });
        //   });
        //   wss.on('error', function() {});
        // }

        /*'verifyClient gets original request'*/
        {
          let p = ++port
          let verifyClientCalled = false;
          let wss = new WebSocketServer({port: p, verifyClient: function(info) {
            assert.equal(info.req.headers['sec-websocket-key1'], '3e6b263  4 17 80');
            verifyClientCalled = true;
            return false;
          }}, function() {
            let options = {
              port: p,
              host: '127.0.0.1',
              headers: {
                'Connection': 'Upgrade',
                'Upgrade': 'WebSocket',
                'Origin': 'http://foobarbaz.com',
                'Sec-WebSocket-Key1': '3e6b263  4 17 80',
                'Sec-WebSocket-Key2': '17  9 G`ZD9   2 2b 7X 3 /r90'
              }
            };
            let req = http.request(options);
            req.write('WjN}|M(6');
            req.end();
            req.on('response', function(res) {
              assert.ok(verifyClientCalled);
              wss.close();
            });
          });
          wss.on('error', function() {});
        }

        /*'client can be denied asynchronously'*/
        {
          let p = ++port
          let wss = new WebSocketServer({port: p, verifyClient: function(o, cb) {
            cb(false);
          }}, function() {
            let options = {
              port: p,
              host: '127.0.0.1',
              headers: {
                'Connection': 'Upgrade',
                'Upgrade': 'WebSocket',
                'Origin': 'http://foobarbaz.com',
                'Sec-WebSocket-Key1': '3e6b263  4 17 80',
                'Sec-WebSocket-Key2': '17  9 G`ZD9   2 2b 7X 3 /r90'
              }
            };
            let req = http.request(options);
            req.write('WjN}|M(6');
            req.end();
            req.on('response', function(res) {
              assert.equal(res.statusCode, 401);
              process.nextTick(function() {
                wss.close();
              });
            });
          });
          wss.on('connection', function(ws) {
            // done(new Error('connection must not be established'));
          });
          wss.on('error', function() {});
        }

        /*'client can be denied asynchronously with custom response code'*/
        // {
        //   let p = ++port
        //   let wss = new WebSocketServer({port: p, verifyClient: function(o, cb) {
        //     cb(false, 404, 'Not Found');
        //   }}, function() {
        //     let options = {
        //       port: p,
        //       host: '127.0.0.1',
        //       headers: {
        //         'Connection': 'Upgrade',
        //         'Upgrade': 'WebSocket',
        //         'Origin': 'http://foobarbaz.com',
        //         'Sec-WebSocket-Key1': '3e6b263  4 17 80',
        //         'Sec-WebSocket-Key2': '17  9 G`ZD9   2 2b 7X 3 /r90'
        //       }
        //     };
        //     let req = http.request(options);
        //     req.write('WjN}|M(6');
        //     req.end();
        //     req.on('response', function(res) {
        //       assert.equal(res.statusCode, 404);
        //       process.nextTick(function() {
        //         wss.close();
        //       });
        //     });
        //   });
        //   wss.on('connection', function(ws) {
        //     // done(new Error('connection must not be established'));
        //   });
        //   wss.on('error', function() {});
        // }

        /*'client can be accepted asynchronously'*/
        // {
        //   let p = ++port
        //   let wss = new WebSocketServer({port: p, verifyClient: function(o, cb) {
        //     cb(true);
        //   }}, function() {
        //     let options = {
        //       port: p,
        //       host: '127.0.0.1',
        //       headers: {
        //         'Connection': 'Upgrade',
        //         'Upgrade': 'WebSocket',
        //         'Origin': 'http://foobarbaz.com',
        //         'Sec-WebSocket-Key1': '3e6b263  4 17 80',
        //         'Sec-WebSocket-Key2': '17  9 G`ZD9   2 2b 7X 3 /r90'
        //       }
        //     };
        //     let req = http.request(options);
        //     req.write('WjN}|M(6');
        //     req.end();
        //   });
        //   wss.on('connection', function(ws) {
        //     wss.close();
        //   });
        //   wss.on('error', function() {});
        // }

        /*'handles messages passed along with the upgrade request (upgrade head)'*/
        // {
        //   let p = ++port
        //   let wss = new WebSocketServer({port: p, verifyClient: function(o) {
        //     return true;
        //   }}, function() {
        //     let options = {
        //       port: p,
        //       host: '127.0.0.1',
        //       headers: {
        //         'Connection': 'Upgrade',
        //         'Upgrade': 'WebSocket',
        //         'Sec-WebSocket-Key1': '3e6b263  4 17 80',
        //         'Sec-WebSocket-Key2': '17  9 G`ZD9   2 2b 7X 3 /r90',
        //         'Origin': 'http://foobar.com'
        //       }
        //     };
        //     let req = http.request(options);
        //     req.write('WjN}|M(6');
        //     req.write(new Buffer([0x00, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0xff], 'binary'));
        //     req.end();
        //   });
        //   wss.on('connection', function(ws) {
        //     ws.on('message', function(data) {
        //       assert.equal(data, 'Hello');
        //       ws.terminate();
        //       wss.close();
        //     });
        //   });
        //   wss.on('error', function() {});
        // }
      }
    }

    /*'client properties'*/
    {
      /*'protocol is exposed'*/
      // {
      //   let p = ++port
      //   let wss = new WebSocketServer({port: p}, function() {
      //     var ws = new WebSocket('ws://localhost:' + p, 'hi');
      //   });
      //   wss.on('connection', function(client) {
      //     assert.equal(client.protocol, 'hi');
      //     wss.close();
      //   });
      // }

      /*'protocolVersion is exposed'*/
      // {
      //   let p = ++port
      //   let wss = new WebSocketServer({port: p}, function() {
      //     var ws = new WebSocket('ws://localhost:' + p, {protocolVersion: 8});
      //   });
      //   wss.on('connection', function(client) {
      //     assert.equal(client.protocolVersion, 8);
      //     wss.close();
      //   });
      // }

      /*'upgradeReq is the original request object'*/
      // {
      //   let p = ++port
      //   let wss = new WebSocketServer({port: p}, function() {
      //     var ws = new WebSocket('ws://localhost:' + p, {protocolVersion: 8});
      //   });
      //   wss.on('connection', function(client) {
      //     assert.equal(client.upgradeReq.httpVersion, '1.1');
      //     wss.close();
      //   });
      // }
    }

    /*'permessage-deflate'*/
    {
      /*'accept connections with permessage-deflate extension'*/
      // {
      //   let p = ++port
      //   let wss = new WebSocketServer({port: p}, function() {
      //     let options = {
      //       port: p,
      //       host: '127.0.0.1',
      //       headers: {
      //         'Connection': 'Upgrade',
      //         'Upgrade': 'websocket',
      //         'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
      //         'Sec-WebSocket-Version': 13,
      //         'Sec-WebSocket-Extensions': 'permessage-deflate; client_max_window_bits=8; server_max_window_bits=8; client_no_context_takeover; server_no_context_takeover'
      //       }
      //     };
      //     let req = http.request(options);
      //     req.end();
      //   });
      //   wss.on('connection', function(ws) {
      //     ws.terminate();
      //     wss.close();
      //   });
      //   wss.on('error', function() {});
      // }

      /*'does not accept connections with not defined extension parameter'*/
      // {
      //   let p = ++port
      //   let wss = new WebSocketServer({port: p}, function() {
      //     let options = {
      //       port: p,
      //       host: '127.0.0.1',
      //       headers: {
      //         'Connection': 'Upgrade',
      //         'Upgrade': 'websocket',
      //         'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
      //         'Sec-WebSocket-Version': 13,
      //         'Sec-WebSocket-Extensions': 'permessage-deflate; foo=15'
      //       }
      //     };
      //     let req = http.request(options);
      //     req.end();
      //     req.on('response', function(res) {
      //       assert.equal(res.statusCode, 400);
      //       wss.close();
      //     });
      //   });
      //   wss.on('connection', function(ws) {
      //     // done(new Error('connection must not be established'));
      //   });
      //   wss.on('error', function() {});
      // }

      /*'does not accept connections with invalid extension parameter'*/
      // {
      //   let p = ++port
      //   let wss = new WebSocketServer({port: p}, function() {
      //     let options = {
      //       port: p,
      //       host: '127.0.0.1',
      //       headers: {
      //         'Connection': 'Upgrade',
      //         'Upgrade': 'websocket',
      //         'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
      //         'Sec-WebSocket-Version': 13,
      //         'Sec-WebSocket-Extensions': 'permessage-deflate; server_max_window_bits=foo'
      //       }
      //     };
      //     let req = http.request(options);
      //     req.end();
      //     req.on('response', function(res) {
      //       assert.equal(res.statusCode, 400);
      //       wss.close();
      //     });
      //   });
      //   wss.on('connection', function(ws) {
      //     // done(new Error('connection must not be established'));
      //   });
      //   wss.on('error', function() {});
      // }
    }
  }
