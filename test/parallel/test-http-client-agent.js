'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

let name;
const max = 3;
let count = 0;

const server = http.Server(common.mustCall((req, res) => {
  if (req.url === '/0') {
    setTimeout(common.mustCall(() => {
      res.writeHead(200);
      res.end('Hello, World!');
    }), 100);
  } else {
    res.writeHead(200);
    res.end('Hello, World!');
  }
}, max));
server.listen(0, common.mustCall(() => {
  name = http.globalAgent.getName({ port: server.address().port });
  for (let i = 0; i < max; ++i)
    request(i);
}));

function request(i) {
  const req = http.get({
    port: server.address().port,
    path: `/${i}`
  }, function(res) {
    const socket = req.socket;
    socket.on('close', common.mustCall(() => {
      ++count;
      if (count < max) {
        assert.strictEqual(http.globalAgent.sockets[name].includes(socket),
                           false);
      } else {
        assert(!http.globalAgent.sockets.hasOwnProperty(name));
        assert(!http.globalAgent.requests.hasOwnProperty(name));
        server.close();
      }
    }));
    res.resume();
  });
}
