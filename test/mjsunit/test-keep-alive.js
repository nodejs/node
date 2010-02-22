// This test requires the program "ab"
process.mixin(require("./common"));
http = require("http");
sys = require("sys");
PORT = 8891;

body = "hello world\n";
server = http.createServer(function (req, res) {
  res.writeHeader(200, { 
    "Content-Length": body.length, 
    "Content-Type": "text/plain", 
  });
  res.write(body);
  res.close();
});
server.listen(PORT);

var keepAliveReqSec = 0;
var normalReqSec = 0;


function runAb(opts, callback) {
  var command = "ab " + opts + " http://127.0.0.1:" + PORT + "/";
  sys.exec(command, function (err, stdout, stderr) {
    if (err) {
      puts("ab not installed? skipping test.\n" + stderr);
      process.exit();
      return;
    }
    if (err) throw err;
    var matches = /Requests per second:\s*(\d+)\./mi.exec(stdout);
    var reqSec = parseInt(matches[1]);

    matches = /Keep-Alive requests:\s*(\d+)/mi.exec(stdout);
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
