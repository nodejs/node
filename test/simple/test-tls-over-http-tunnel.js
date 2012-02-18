// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.




if (!process.versions.openssl) {
  console.error('Skipping because node compiled without OpenSSL.');
  process.exit(0);
}

var common = require('../common');
var assert = require('assert');

var fs = require('fs');
var net = require('net');
var http = require('http');
var https = require('https');

var proxyPort = common.PORT + 1;
var gotRequest = false;

var key = fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem');
var cert = fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem');

var options = {
  key: key,
  cert: cert
};

var server = https.createServer(options, function(req, res) {
  console.log('SERVER: got request');
  res.writeHead(200, {
    'content-type': 'text/plain',
   });
  console.log('SERVER: sending response');
  res.end('hello world\n');
});

var proxy = net.createServer(function(clientSocket) {
  console.log('PROXY: got a client connection');

  var serverSocket = null;

  clientSocket.on('data', function(chunk) {
    if (!serverSocket) {
      // Verify the CONNECT request
      assert.equal('CONNECT localhost:' + common.PORT + ' HTTP/1.1\r\n' + 
                   'Proxy-Connections: keep-alive\r\nContent-Length:'   +
                   ' 0\r\nHost: localhost:' + proxyPort + '\r\n\r\n',
                   chunk);

      console.log('PROXY: got CONNECT request');
      console.log('PROXY: creating a tunnel');

      // create the tunnel
      serverSocket = net.connect(common.PORT, function() {
        console.log('PROXY: replying to client CONNECT request');

        // Send the response
        clientSocket.write('HTTP/1.1 200 OK\r\nProxy-Connections: keep' +
                           '-alive\r\nConnections: keep-alive\r\nVia: ' +
                           'localhost:' + proxyPort + '\r\n\r\n');
      });

      serverSocket.on('data', function(chunk) {
        clientSocket.write(chunk);
      });

      serverSocket.on('end', function() {
        clientSocket.destroy();
      });
    } else {
      serverSocket.write(chunk);
    }
  });

  clientSocket.on('end', function() {
    serverSocket.destroy();
  });
});

server.listen(common.PORT);

proxy.listen(proxyPort, function() {
  console.log('CLIENT: Making CONNECT request');

  http.request({
    port: proxyPort,
    method: 'CONNECT',
    path: 'localhost:' + common.PORT,
    headers: {
      'Proxy-Connections': 'keep-alive',
      'Content-Length': 0
    }
  }, function(res) {
    assert.equal(200, res.statusCode);
    console.log('CLIENT: got CONNECT response');

    // detach the socket
    res.socket.emit('agentRemove');
    res.socket.removeAllListeners('data');
    res.socket.removeAllListeners('close');
    res.socket.removeAllListeners('error');
    res.socket.removeAllListeners('drain');
    res.socket.removeAllListeners('end');
    res.socket.ondata = null;
    res.socket.onend = null;
    res.socket.ondrain = null;

    console.log('CLIENT: Making HTTPS request');

    https.get({
      path: '/foo',
      key: key,
      cert: cert,
      socket: res.socket,  // reuse the socket
      agent: false,
    }, function(res) {
      assert.equal(200, res.statusCode);

      res.on('data', function(chunk) {
        assert.equal('hello world\n', chunk);
        console.log('CLIENT: got HTTPS response');
        gotRequest = true;
      });

      res.on('end', function() {
        proxy.close();
        server.close();
      });
    }).end();
  }).end();
});

process.on('exit', function() {
  assert.ok(gotRequest);
});
