'use strict';
const common = require('../common');
const http = require('http');

let serverRes;
const server = http.Server((req, res) => {
  serverRes = res;
  res.writeHead(200);
  res.write('Part of my res.');

  res.destroy();
});

server.listen(0, common.mustCall(() => {
  http.get({
    port: server.address().port,
    headers: { connection: 'keep-alive' }
  }, common.mustCall((res) => {
    server.close();
    serverRes.destroy();

    res.resume();
    res.on('end', common.mustCall());
    res.on('aborted', common.mustCall());
    res.on('close', common.mustCall());
    res.socket.on('close', common.mustCall());
  }));
}));
