'use strict';
// just a simple http server and client.

function serverHandler(req, res) {
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.end('Hello World\n');
}

const http = require('http');
const weak = require('weak');
const common = require('../common');
const assert = require('assert');
const PORT = common.PORT;
const todo = 500;
let done = 0;
let count = 0;
let countGC = 0;

console.log('We should do ' + todo + ' requests');

var server = http.createServer(serverHandler);
server.listen(PORT, getall);


function getall() {
  if (count >= todo)
    return;

  (function() {
    function cb(res) {
      res.resume();
      console.error('in cb');
      done += 1;
      res.on('end', gc);
    }

    var req = http.get({
      hostname: 'localhost',
      pathname: '/',
      port: PORT
    }, cb);

    count++;
    weak(req, afterGC);
  })();

  setImmediate(getall);
}

for (var i = 0; i < 10; i++)
  getall();

function afterGC() {
  countGC++;
}

setInterval(status, 1000).unref();

function status() {
  gc();
  console.log('Done: %d/%d', done, todo);
  console.log('Collected: %d/%d', countGC, count);
  if (done === todo) {
    console.log('All should be collected now.');
    assert(count === countGC);
    process.exit(0);
  }
}

