'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const Countdown = require('../common/countdown');

const agent = new http.Agent({
  keepAlive: true,
  keepAliveMsecs: 1000,
  maxSockets: 2,
  maxFreeSockets: 2
});

const server = http.createServer(common.mustCall((req, res) => {
  res.end('hello world');
}, 2));

function get(path, callback) {
  return http.get({
    host: 'localhost',
    port: server.address().port,
    agent: agent,
    path: path
  }, callback);
}

const countdown = new Countdown(2, common.mustCall(() => {
  const freepool = agent.freeSockets[Object.keys(agent.freeSockets)[0]];
  assert.strictEqual(freepool.length, 2,
                     `expect keep 2 free sockets, but got ${freepool.length}`);
  agent.destroy();
  server.close();
}));

function dec() {
  process.nextTick(() => countdown.dec());
}

function onGet(res) {
  assert.strictEqual(res.statusCode, 200);
  res.resume();
  res.on('end', common.mustCall(dec));
}

server.listen(0, common.mustCall(() => {
  get('/1', common.mustCall(onGet));
  get('/2', common.mustCall(onGet));
}));
