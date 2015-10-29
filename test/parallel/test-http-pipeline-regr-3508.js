'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

var once = false;
var first = null;
var second = null;

const chunk = new Buffer(1024);
chunk.fill('X');

var size = 0;

var more;
var done;

var server = http.createServer(function(req, res) {
  if (!once)
    server.close();
  once = true;

  if (first === null) {
    first = res;
    return;
  }
  if (second === null) {
    second = res;
    res.write(chunk);
  } else {
    res.end(chunk);
  }
  size += res.outputSize;
  if (size <= req.socket._writableState.highWaterMark) {
    more();
    return;
  }
  done();
}).on('upgrade', function(req, socket) {
  second.end(chunk, function() {
    socket.end();
  });
  first.end('hello');
}).listen(common.PORT, function() {
  var s = net.connect(common.PORT);
  more = function() {
    s.write('GET / HTTP/1.1\r\n\r\n');
  };
  done = function() {
    s.write('GET / HTTP/1.1\r\n\r\n' +
            'GET / HTTP/1.1\r\nConnection: upgrade\r\nUpgrade: ws\r\n\r\naaa');
  };
  more();
  more();
  s.resume();
});
