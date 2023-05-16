'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const childProcess = require('child_process');
const http = require('http');

let port;

const server = http.createServer(common.mustCall((req, res) => {
  if (req.url.startsWith('/json')) {
    res.writeHead(200);
    res.end(`[ {
      "description": "",
      "devtoolsFrontendUrl": "/devtools/inspector.html?ws=localhost:${port}/devtools/page/DAB7FB6187B554E10B0BD18821265734",
      "cid": "DAB7FB6187B554E10B0BD18821265734",
      "title": "Fhqwhgads",
      "type": "page",
      "url": "https://www.example.com/",
      "webSocketDebuggerUrl": "ws://localhost:${port}/devtools/page/DAB7FB6187B554E10B0BD18821265734"
    } ]`);
  } else {
    res.setHeader('Upgrade', 'websocket');
    res.setHeader('Connection', 'Upgrade');
    res.setHeader('Sec-WebSocket-Accept', 'fhqwhgads');
    res.setHeader('Sec-WebSocket-Protocol', 'chat');
    res.writeHead(101);
    res.end();
  }
}, 2)).listen(0, common.mustCall(() => {
  port = server.address().port;
  const proc =
    childProcess.spawn(process.execPath, ['inspect', `localhost:${port}`]);

  let stdout = '';
  proc.stdout.on('data', (data) => {
    stdout += data.toString();
    assert.doesNotMatch(stdout, /\bok\b/);
  });

  let stderr = '';
  proc.stderr.on('data', (data) => {
    stderr += data.toString();
  });

  proc.on('exit', common.mustCall((code, signal) => {
    assert.match(stderr, /\bWebSocket secret mismatch\b/);
    assert.notStrictEqual(code, 0);
    assert.strictEqual(signal, null);
    server.close();
  }));
}));
