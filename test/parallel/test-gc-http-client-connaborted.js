'use strict';
// Flags: --expose-gc
// just like test-gc-http-client.js,
// but aborting every connection that comes in.

const common = require('../common');
const onGC = require('../common/ongc');

const http = require('http');
const todo = 500;
let done = 0;
let count = 0;
let countGC = 0;

console.log(`We should do ${todo} requests`);

function serverHandler(req, res) {
  res.connection.destroy();
}

const server = http.createServer(serverHandler);
server.listen(0, common.mustCall(() => {
  for (let i = 0; i < 10; i++)
    getall();
}));

function getall() {
  if (count >= todo)
    return;

  const req = http.get({
    hostname: 'localhost',
    pathname: '/',
    port: server.address().port
  }, cb).on('error', cb);

  count++;
  onGC(req, { ongc });

  setImmediate(getall);
}

function cb(res) {
  done += 1;
}

function ongc() {
  countGC++;
}

setInterval(status, 100).unref();

function status() {
  global.gc();
  console.log('Done: %d/%d', done, todo);
  console.log('Collected: %d/%d', countGC, count);
  if (countGC === todo) server.close();
}
