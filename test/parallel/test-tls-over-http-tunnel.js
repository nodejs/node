var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var https = require('https');

var fs = require('fs');
var net = require('net');
var http = require('http');

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
    'content-type': 'text/plain'
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
                   'Proxy-Connections: keep-alive\r\n' +
                   'Host: localhost:' + proxyPort + '\r\n' +
                   'Connection: close\r\n\r\n',
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

  var req = http.request({
    port: proxyPort,
    method: 'CONNECT',
    path: 'localhost:' + common.PORT,
    headers: {
      'Proxy-Connections': 'keep-alive'
    }
  });
  req.useChunkedEncodingByDefault = false; // for v0.6
  req.on('response', onResponse); // for v0.6
  req.on('upgrade', onUpgrade);   // for v0.6
  req.on('connect', onConnect);   // for v0.7 or later
  req.end();

  function onResponse(res) {
    // Very hacky. This is necessary to avoid http-parser leaks.
    res.upgrade = true;
  }

  function onUpgrade(res, socket, head) {
    // Hacky.
    process.nextTick(function() {
      onConnect(res, socket, head);
    });
  }

  function onConnect(res, socket, header) {
    assert.equal(200, res.statusCode);
    console.log('CLIENT: got CONNECT response');

    // detach the socket
    socket.removeAllListeners('data');
    socket.removeAllListeners('close');
    socket.removeAllListeners('error');
    socket.removeAllListeners('drain');
    socket.removeAllListeners('end');
    socket.ondata = null;
    socket.onend = null;
    socket.ondrain = null;

    console.log('CLIENT: Making HTTPS request');

    https.get({
      path: '/foo',
      key: key,
      cert: cert,
      socket: socket,  // reuse the socket
      agent: false,
      rejectUnauthorized: false
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
    }).on('error', function(er) {
      // We're ok with getting ECONNRESET in this test, but it's
      // timing-dependent, and thus unreliable. Any other errors
      // are just failures, though.
      if (er.code !== 'ECONNRESET')
        throw er;
    }).end();
  }
});

process.on('exit', function() {
  assert.ok(gotRequest);
});
