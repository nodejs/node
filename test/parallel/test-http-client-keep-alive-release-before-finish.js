'use strict';
const common = require('../common');
const http = require('http');

const server = http.createServer((req, res) => {
  res.end();
}).listen(0, common.mustCall(() => {
  const agent = new http.Agent({
    maxSockets: 1,
    keepAlive: true
  });

  const port = server.address().port;

  const post = http.request({
    agent,
    method: 'POST',
    port,
  }, common.mustCall((res) => {
    res.resume();
  }));

  /* What happens here is that the server `end`s the response before we send
   * `something`, and the client thought that this is a green light for sending
   * next GET request
   */
  post.write(Buffer.alloc(16 * 1024, 'X'));
  setTimeout(() => {
    post.end('something');
  }, 100);

  http.request({
    agent,
    method: 'GET',
    port,
  }, common.mustCall((res) => {
    server.close();
    res.connection.end();
  })).end();
}));
