require("../common");
var http = require("http"),
  sys = require("sys"),
  server,
  server_response = "Thank you, come again.",
  client_requests = [],
  cur, timer, req_num, exception_count = 0;

server = http.createServer(function (req, res) {
  intentionally_not_defined();
  res.writeHead(200, {"Content-Type": "text/plain"});
  res.write(server_response);
  res.end();
});
server.listen(PORT);

function check_reqs() {
  var done_reqs = 0;
  client_requests.forEach(function (v) {
    if (v.done) {
      done_reqs += 1;
    }
  });
  if (done_reqs === 4) {
    sys.puts("Got all requests, which is bad.");
    clearTimeout(timer);
  }
}

function add_client(num) {
  var req = http.createClient(PORT).request('GET', '/busy/' + num);

  req.addListener('response', function(res) {
    var response_body = "";
    res.setEncoding("utf8");
    res.addListener('data', function(chunk) {
      response_body += chunk;
    });
    res.addListener('end', function() {
      assert.strictEqual(response_body, server_response);
      req.done = true;
      check_reqs();
    });
  });
  req.end();

  return req;
}

for (req_num = 0; req_num < 4 ; req_num += 1) {
  client_requests.push(add_client(req_num));
}

function exception_handler(err) {
  sys.puts("Caught an exception: " + err);
  if (err.name === "AssertionError") {
    throw(err);
  }
  exception_count += 1;
}
process.addListener("uncaughtException", exception_handler);

timer = setTimeout(function () {
  process.removeListener("uncaughtException", exception_handler);
  server.close();
  assert.strictEqual(4, exception_count);
  process.exit(0);
}, 300);
