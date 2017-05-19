'use strict';
const { tagCRLFy } = require('../common');
const http = require('http');
const net = require('net');

let once = false;
let first = null;
let second = null;

const chunk = Buffer.alloc(1024, 'X');

let size = 0;

let more;
let done;

const server = http.createServer(function(req, res) {
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
}).listen(0, function() {
  const s = net.connect(this.address().port);
  more = function() {
    s.write('GET / HTTP/1.1\r\n\r\n');
  };
  done = function() {
    s.write(tagCRLFy`
      GET / HTTP/1.1

      GET / HTTP/1.1
      Connection: upgrade
      Upgrade: ws

      aaa
    `);
  };
  more();
  more();
  s.resume();
});
