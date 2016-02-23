'use strict';
// just like test/gc/http-client.js,
// but aborting every connection that comes in.

function serverHandler(req, res) {
  res.connection.destroy();
}

const http  = require('http');
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
      done += 1;
      statusLater();
    }

    var req = http.get({
      hostname: 'localhost',
      pathname: '/',
      port: PORT
    }, cb).on('error', cb);

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

var timer;
function statusLater() {
  gc();
  if (timer) clearTimeout(timer);
  timer = setTimeout(status, 1);
}

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
