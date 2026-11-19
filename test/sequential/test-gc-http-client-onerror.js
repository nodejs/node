'use strict';
// Flags: --expose-gc
// just like test-gc-http-client.js,
// but with an on('error') handler that rethrows.

const common = require('../common');
const { onGC } = require('../common/gc');

function serverHandler(req, res) {
  req.resume();
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('Hello World\n');
}

const http = require('http');
// Keep the number of in-flight requests small and fixed. Opening too many
// connections at once can overflow the listen queue (capped at 128 on macOS),
// and the kernel then resets connections that are still being established.
const concurrency = 8;
const numRequests = 64;
let count = 0;
let done = 0;
let countGC = 0;

const server = http.createServer(serverHandler);
server.listen(0, common.mustCall(() => {
  for (let i = 0; i < concurrency; i++)
    getNext();
}));

function getNext() {
  if (count >= numRequests)
    return;

  const req = http.get({
    hostname: 'localhost',
    pathname: '/',
    port: server.address().port,
  }, cb).on('error', onerror);

  count++;
  onGC(req, { ongc });
}

function cb(res) {
  res.resume();
  res.on('end', () => {
    done++;
    getNext();
  });
}

function onerror(err) {
  throw err;
}

function ongc() {
  countGC++;
}

setImmediate(status);

function status() {
  if (done === numRequests) {
    global.gc();
    console.log(`done/collected/total: ${done}/${countGC}/${numRequests}`);
    if (countGC === numRequests) {
      server.close();
      return;
    }
  }

  setImmediate(status);
}
