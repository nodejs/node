'use strict';
const common = require('../common');
const asyncHooks = require('async_hooks');
const http = require('http');

// Regression test for https://github.com/nodejs/node/issues/31796

asyncHooks.createHook({
  after: () => {}
}).enable();


process.once('uncaughtException', common.mustCall(() => {
  server.close();
}));

const server = http.createServer(common.mustCall((request, response) => {
  response.writeHead(200, { 'Content-Type': 'text/plain' });
  response.end();
}));

server.listen(0, common.mustCall(() => {
  http.get({
    host: 'localhost',
    port: server.address().port
  }, common.mustCall(() => {
    throw new Error('whoah');
  }));
}));
