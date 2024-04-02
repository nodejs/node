'use strict';
require('../common');

// This test ensures that Node.js doesn't crash because of a TypeError by
// checking in `connectionListener` that the socket still has the parser.
// https://github.com/nodejs/node/issues/3508

const http = require('http');
const net = require('net');

let once = false;
let first = null;
let second = null;

const chunk = Buffer.alloc(1024, 'X');

let size = 0;

let more;
let done;

const server = http
  .createServer((req, res) => {
    if (!once) server.close();
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
    if (size <= req.socket.writableHighWaterMark) {
      more();
      return;
    }
    done();
  })
  .on('upgrade', (req, socket) => {
    second.end(chunk, () => {
      socket.end();
    });
    first.end('hello');
  })
  .listen(0, () => {
    const s = net.connect(server.address().port);
    more = () => {
      s.write('GET / HTTP/1.1\r\n\r\n');
    };
    done = () => {
      s.write(
        'GET / HTTP/1.1\r\n\r\n' +
          'GET / HTTP/1.1\r\nConnection: upgrade\r\nUpgrade: ws\r\n\r\naaa'
      );
    };
    more();
    more();
    s.resume();
  });
