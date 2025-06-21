'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const net = require('net');
const http = require('http');
const http2 = require('http2');

// Example test for HTTP/1 vs HTTP/2 protocol autoselection.
// Refs: https://github.com/nodejs/node/issues/34532

const h1Server = http.createServer(common.mustCall((req, res) => {
  res.end('HTTP/1 Response');
}));

const h2Server = http2.createServer(common.mustCall((req, res) => {
  res.end('HTTP/2 Response');
}));

const rawServer = net.createServer(common.mustCall(function listener(socket) {
  const data = socket.read(3);

  if (!data) { // Repeat until data is available
    socket.once('readable', () => listener(socket));
    return;
  }

  // Put the data back, so the real server can handle it:
  socket.unshift(data);

  if (data.toString('ascii') === 'PRI') { // Very dumb preface check
    h2Server.emit('connection', socket);
  } else {
    h1Server.emit('connection', socket);
  }
}, 2));

rawServer.listen(common.mustCall(() => {
  const { port } = rawServer.address();

  let done = 0;
  {
    // HTTP/2 Request
    const client = http2.connect(`http://localhost:${port}`);
    const req = client.request({ ':path': '/' });
    req.end();

    let content = '';
    req.setEncoding('utf8');
    req.on('data', (chunk) => content += chunk);
    req.on('end', common.mustCall(() => {
      assert.strictEqual(content, 'HTTP/2 Response');
      if (++done === 2) rawServer.close();
      client.close();
    }));
  }

  {
    // HTTP/1 Request
    http.get(`http://localhost:${port}`, common.mustCall((res) => {
      let content = '';
      res.setEncoding('utf8');
      res.on('data', (chunk) => content += chunk);
      res.on('end', common.mustCall(() => {
        assert.strictEqual(content, 'HTTP/1 Response');
        if (++done === 2) rawServer.close();
      }));
    }));
  }
}));
