'use strict';
// Flags: --expose-gc
// just a simple http server and client.

const common = require('../common');
const onGC = require('../common/ongc');

const cpus = require('os').cpus().length;

function serverHandler(req, res) {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('Hello World\n');
}

const http = require('http');
let createClients = true;
let done = 0;
let count = 0;
let countGC = 0;

const server = http.createServer(serverHandler);
server.listen(0, common.mustCall(() => {
  for (let i = 0; i < cpus; i++)
    getAll();
}));

function getAll() {
  if (!createClients)
    return;

  const req = http.get({
    hostname: 'localhost',
    pathname: '/',
    port: server.address().port
  }, cb);

  count++;
  onGC(req, { ongc });

  setImmediate(getAll);
}

function cb(res) {
  res.resume();
  done += 1;
}

function ongc() {
  countGC++;
}

setImmediate(status);

function status() {
  if (done > 0) {
    createClients = false;
    global.gc();
    console.log(`done/collected/total: ${done}/${countGC}/${count}`);
    if (countGC === count) {
      server.close();
    } else {
      setImmediate(status);
    }
  } else {
    setImmediate(status);
  }
}
