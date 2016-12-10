'use strict';
// just like test/gc/http-client.js,
// but with a timeout set

require('../common');

function serverHandler(req, res) {
  setTimeout(function() {
    req.resume();
    res.writeHead(200);
    res.end('hello\n');
  }, 100);
}

const http = require('http');
const weak = require('weak');
const assert = require('assert');
const todo = 550;
let done = 0;
let count = 0;
let countGC = 0;

console.log('We should do ' + todo + ' requests');

var server = http.createServer(serverHandler);
server.listen(0, getall);

function getall() {
  if (count >= todo)
    return;

  (function() {
    function cb(res) {
      res.resume();
      done += 1;
      statusLater();
    }

    var req = http.get({
      hostname: 'localhost',
      pathname: '/',
      port: server.address().port
    }, cb);
    req.on('error', cb);
    req.setTimeout(10, function() {
      console.log('timeout (expected)');
    });

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
  global.gc();
  if (timer) clearTimeout(timer);
  timer = setTimeout(status, 1);
}

function status() {
  global.gc();
  console.log('Done: %d/%d', done, todo);
  console.log('Collected: %d/%d', countGC, count);
  if (done === todo) {
    console.log('All should be collected now.');
    assert.strictEqual(count, countGC);
    process.exit(0);
  }
}
