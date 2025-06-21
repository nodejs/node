'use strict';

const common = require('../common');
const http = require('http');

function request(server) {
  http.get({
    port: server.address().port,
    path: '/',
  }, (res) => {
    res.resume();
  });
}

{
  const server = http.createServer((req, res) => {
    //  Hack to not remove parser out of server.connectionList
    //  See `freeParser` in _http_common.js
    req.socket.parser.free = common.mustCall();
    req.socket.on('close', common.mustCall(() => {
      server.close();
    }));
    res.end('ok');
  }).listen(0, common.mustCall(() => {
    request(server);
  }));
}

{
  const server = http.createServer((req, res) => {
    // See `freeParser` in _http_common.js
    const { parser } = req.socket;
    parser.free = common.mustCall(() => {
      setImmediate(common.mustCall(() => {
        parser.close();
      }));
    });
    req.socket.on('close', common.mustCall(() => {
      setImmediate(common.mustCall(() => {
        server.close();
      }));
    }));
    res.end('ok');
  }).listen(0, common.mustCall(() => {
    request(server);
  }));
}
