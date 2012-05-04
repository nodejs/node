// just like test/gc/http-client.js,
// but aborting every connection that comes in.

function serverHandler(req, res) {
  res.connection.destroy();
}

var http  = require('http'),
    weak    = require('weak'),
    done    = 0,
    count   = 0,
    countGC = 0,
    todo    = 500,
    common = require('../common.js'),
    assert = require('assert'),
    PORT = common.PORT;

console.log('We should do '+ todo +' requests');

var http = require('http');
var server = http.createServer(serverHandler);
server.listen(PORT, getall);

function getall() {
  for (var i = 0; i < todo; i++) {
    (function(){
      function cb(res) {
        done+=1;
        statusLater();
      }

      var req = http.get({
        hostname: 'localhost',
        pathname: '/',
        port: PORT
      }, cb).on('error', cb);

      count++;
      weak(req, afterGC);
    })()
  }
}

function afterGC(){
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
    assert(count === countGC);
    process.exit(0);
  }
}
