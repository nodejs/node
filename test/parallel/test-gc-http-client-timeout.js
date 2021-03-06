'use strict';
// Flags: --expose-gc
// just like test-gc-http-client.js,
// but with a timeout set

const common = require('../common');
const onGC = require('../common/ongc');

function serverHandler(req, res) {
  setTimeout(function() {
    req.resume();
    res.writeHead(200);
    res.end('hello\n');
  }, 100);
}

const http = require('http');
const todo = 300;
let done = 0;
let count = 0;
let countGC = 0;

console.log(`We should do ${todo} requests`);

const server = http.createServer(serverHandler);
server.listen(0, common.mustCall(getall));

function getall() {
  if (count >= todo)
    return;

  const req = http.get({
    hostname: 'localhost',
    pathname: '/',
    port: server.address().port
  }, cb);

  req.setTimeout(10, function() {
    console.log('timeout (expected)');
  });

  count++;
  onGC(req, { ongc });

  setImmediate(getall);
}

for (let i = 0; i < 10; i++)
  getall();

function cb(res) {
  res.resume();
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
