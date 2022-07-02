'use strict';
// Flags: --expose-gc
// just like test-gc-http-client.js,
// but with a timeout set

const common = require('../common');
const onGC = require('../common/ongc');
const http = require('http');
const os = require('os');

function serverHandler(req, res) {
  setTimeout(function() {
    req.resume();
    res.writeHead(200);
    res.end('hello\n');
  }, 100);
}

const cpus = os.cpus().length;
let createClients = true;
let done = 0;
let count = 0;
let countGC = 0;

const server = http.createServer(serverHandler);
server.listen(0, common.mustCall(getAll));

function getAll() {
  if (!createClients)
    return;

  const req = http.get({
    hostname: 'localhost',
    pathname: '/',
    port: server.address().port
  }, cb);

  req.setTimeout(10, common.mustCall());

  count++;
  onGC(req, { ongc });

  setImmediate(getAll);
}

for (let i = 0; i < cpus; i++)
  getAll();

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
      return;
    }
  }

  setImmediate(status);
}
