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
ensure
  `kill -9 #{pid}`
end

__END__
var server = new HTTPServer(null, 8000, function (request) {
  log("request");
  request.respond("HTTP/1.1 200 OK\r\n");
  request.respond("Content-Length: 0\r\n");
  request.respond("\r\n");
  request.respond(null);
});
