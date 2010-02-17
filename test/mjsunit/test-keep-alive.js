// This test requires the program "ab"
process.mixin(require("./common"));
http = require("http");
sys = require("sys");
PORT = 8891;

body = "hello world\n";
server = http.createServer(function (req, res) {
  res.sendHeader(200, { 
    "Content-Length": body.length, 
    "Content-Type": "text/plain", 
  });
  res.write(body);
  res.close();
});
server.listen(PORT);

var keepAliveReqSec = 0;
var normalReqSec = 0;

function error (msg) {
  throw new Error("ERROR. 'ab' not installed? " + msg);
}

function runAb(opts, callback) {
  sys.exec("ab " + opts + " http://127.0.0.1:" + PORT + "/")
    .addErrback(error)
    .addCallback(function (out) {
      var matches = /Requests per second:\s*(\d+)\./mi.exec(out);
      var reqSec = parseInt(matches[1]);

      matches = /Keep-Alive requests:\s*(\d+)/mi.exec(out);
      var keepAliveRequests;
      if (matches) {
        keepAliveRequests = parseInt(matches[1]);
      } else {
        keepAliveRequests = 0;
      }

      callback(reqSec, keepAliveRequests);
    });
}

runAb("-k -c 100 -t 2", function (reqSec, keepAliveRequests) {
  keepAliveReqSec = reqSec;
  assert.equal(true, keepAliveRequests > 0);
  puts("keep-alive: " + keepAliveReqSec + " req/sec");

  runAb("-c 100 -t 2", function (reqSec, keepAliveRequests) {
    normalReqSec = reqSec;
    assert.equal(0, keepAliveRequests);
    puts("normal: " + normalReqSec + " req/sec");
    server.close();
  });
});

process.addListener("exit", function () {
  assert.equal(true, normalReqSec > 50);
  assert.equal(true, keepAliveReqSec > 50);
  assert.equal(true, normalReqSec < keepAliveReqSec);
});
