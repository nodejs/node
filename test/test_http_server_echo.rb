#!/usr/bin/env ruby
require 'net/http'
require File.dirname(__FILE__) + "/common"

pid = fork do
  exec($node, $tf.path)
end

begin 
  wait_for_server("localhost", 8000)
  connection = Net::HTTP.new("localhost", 8000)

  response, body = connection.get("/")
  assert_equal(response.code, "200")
  assert(response.chunked?)
  assert_equal(body, "\n")
  assert_equal(response.content_type, "text/plain")

  response, body = connection.post("/", "hello world")
  assert_equal(response.code, "200")
  assert(response.chunked?)
  assert_equal(body, "hello world\n")
  assert_equal(response.content_type, "text/plain")
ensure
  `kill -9 #{pid}`
end

__END__
function encode(data) { 
  var chunk = data.toString();
  return chunk.length.toString(16) + "\r\n" + chunk + "\r\n";
}

var port = 8000;
var server = new HTTPServer("localhost", port, function (request) {

  // onBody sends null on the last chunk.
  request.onbody = function (chunk) {
    if(chunk) { 
      this.respond(encode(chunk));
    } else {
      this.respond(encode("\n"));
      this.respond("0\r\n\r\n");
      this.respond(null); // signals end-of-request
    }
  }
  request.respond("HTTP/1.0 200 OK\r\n");
  request.respond("Content-Type: text/plain\r\n");
  request.respond("Transfer-Encoding: chunked\r\n");
  request.respond("\r\n");
});

log("Running at http://localhost:" + port + "/");
