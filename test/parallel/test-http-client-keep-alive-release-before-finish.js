'use strict';
const common = require('../common');
const http = require('http');

const server = http.createServer((req, res) => {
  res.end();
}).listen(common.PORT, common.mustCall(() => {
  const agent = new http.Agent({
    maxSockets: 1,
    keepAlive: true
  });

  const post = http.request({
    agent: agent,
    method: 'POST',
    port: common.PORT,
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
    agent: agent,
    method: 'GET',
    port: common.PORT,
  }, common.mustCall((res) => {
    server.close();
    res.connection.end();
  })).end();
}));
