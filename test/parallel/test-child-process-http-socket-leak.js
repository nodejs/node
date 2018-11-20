// Flags: --expose_internals

'use strict';

const common = require('../common');
const assert = require('assert');
const { fork } = require('child_process');
const http = require('http');

if (process.argv[2] === 'child') {
  process.once('message', (req, socket) => {
    const res = new http.ServerResponse(req);
    res.assignSocket(socket);
    res.end();
  });

  process.send('ready');
  return;
}

const { kTimeout } = require('internal/timers');

let child;
let socket;

const server = http.createServer(common.mustCall((req, res) => {
  const r = {
    method: req.method,
    headers: req.headers,
    path: req.path,
    httpVersionMajor: req.httpVersionMajor,
    query: req.query,
  };

  socket = res.socket;
  child.send(r, socket);
  server.close();
}));

server.listen(0, common.mustCall(() => {
  child = fork(__filename, [ 'child' ]);
  child.once('message', (msg) => {
    assert.strictEqual(msg, 'ready');
    const req = http.request({
      port: server.address().port,
    }, common.mustCall((res) => {
      res.on('data', () => {});
      res.on('end', common.mustCall(() => {
        assert.strictEqual(socket[kTimeout]._idleTimeout, -1);
        assert.strictEqual(socket.parser, null);
        assert.strictEqual(socket._httpMessage, null);
      }));
    }));

    req.end();
  });
}));
