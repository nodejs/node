// just a simple http server and client.

function serverHandler(req, res) {
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

console.log('We should do '+ todo +' requests');

var http = require('http');
var server = http.createServer(serverHandler);
server.listen(PORT, getall);


function getall() {
  if (count >= todo)
    return;

  (function(){
    function cb(res) {
      res.resume();
      console.error('in cb')
      done+=1;
      res.on('end', gc);
    }

    var req = http.get({
      hostname: 'localhost',
      pathname: '/',
      port: PORT
    }, cb)

    count++;
    weak(req, afterGC);
  })()

  setImmediate(getall);
}

for (var i = 0; i < 10; i++)
  getall();

function afterGC(){
  countGC ++;
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

