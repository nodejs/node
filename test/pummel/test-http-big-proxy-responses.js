require('../common');
var sys = require("sys"),
fs = require("fs"),
http = require("http"),
url = require("url");

// Produce a very large response.
var chargen = http.createServer(function (req, res) {
    var chunk = '01234567890123456789';
    var len = req.headers['x-len'];
    res.writeHead(200, {"transfer-encoding":"chunked"});
    for (var i=0; i<len; i++) {
  res.write(chunk);
    }
    res.end();
});
chargen.listen(9000);

// Proxy to the chargen server.
var proxy = http.createServer(function (req, res) {
    var proxy_req = http.createClient(9000, 'localhost')
  .request(req.method, req.url, req.headers);
    proxy_req.addListener('response', function(proxy_res) {
  res.writeHead(proxy_res.statusCode, proxy_res.headers);
  proxy_res.addListener('data', function(chunk) {
      res.write(chunk);
  });
  proxy_res.addListener('end', function() {
      res.end();
  });
    });
    proxy_req.end();
});
proxy.listen(9001);

var done = false; 

function call_chargen(list) {
  if (list.length > 0) {
    sys.debug("calling chargen for " + list[0] + " chunks.");
    var req = http.createClient(9001, 'localhost').request('/', {'x-len': list[0]});
    req.addListener('response', function(res) {
      res.addListener('end', function() {
        sys.debug("end for " + list[0] + " chunks.");
        list.shift();
        call_chargen(list);
      });
    });
    req.end();
  }
  else {
    sys.puts("End of list.");
      proxy.end();
      chargen.end();
      done = true;
  }
}

call_chargen([ 100, 1000, 10000, 100000, 1000000 ]);

process.addListener('exit', function () {
  assert.ok(done);
});
