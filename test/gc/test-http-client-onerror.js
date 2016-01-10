'use strict';
// just like test/gc/http-client.js,
// but with an on('error') handler that does nothing.

function serverHandler(req, res) {
  req.resume();
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.end('Hello World\n');
}

var http  = require('http'),
    weak    = require('weak'),
    done    = 0,
    count   = 0,
    countGC = 0,
    todo    = 500,
    common = require('../common'),
    assert = require('assert'),
    PORT = common.PORT;

console.log('We should do ' + todo + ' requests');

var server = http.createServer(serverHandler);
server.listen(PORT, runTest);

function getall() {
  if (count >= todo)
    return;

  (function() {
    function cb(res) {
      res.resume();
      done += 1;
      statusLater();
    }
    function onerror(er) {
      throw er;
    }

    var req = http.get({
      hostname: 'localhost',
      pathname: '/',
      port: PORT
    }, cb).on('error', onerror);

    count++;
    weak(req, afterGC);
  })();

  setImmediate(getall);
}

function runTest() {
  for (var i = 0; i < 10; i++)
    getall();
}

function afterGC() {
  countGC ++;
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
    assert.strictEqual(count, countGC);
    process.exit(0);
  }
}

