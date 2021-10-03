'use strict';

require('../common');
const http = require('http');
const net = require('net');
const stream = require('stream');

const runPipeline1 = () => {
  const s1 = new stream.PassThrough();
  const s2 = new stream.PassThrough();

  s1.end('test');

  const promise = new Promise((resolve, reject) => {
    stream.pipeline(s1, s2, s1, (err) => {
      if (err) return reject(err);
      resolve();
    });

    setTimeout(() => reject(new Error('Pipeline timed out')), 1e3);
  });

  return promise;
};

const runPipeline2 = () => {
  const server = http.createServer();

  const promise = new Promise((resolve, reject) => {
    server.once('upgrade', (req, socket) => {
      setTimeout(() => socket.end(), 0);

      stream.pipeline(
        socket,
        new stream.PassThrough(),
        socket,
        (err) => {
          if (err) return reject(err);
          resolve();
        },
      );
    });

    setTimeout(() => reject(new Error('Pipeline timed out')), 1e3);
  });

  server.listen(0, () => {
    const conn = net
      .createConnection(server.address().port)
      .setEncoding('utf8');

    conn.once('connect', () => {
      conn.write([
        'GET / HTTP/1.1',
        'Upgrade: WebSocket',
        'Connection: Upgrade',
        '\r\n',
      ].join('\r\n'));
    });
  });

  return promise;
};

Promise.all([
  runPipeline1(),
  runPipeline2(),
]).catch((err) => {
  console.error(err);
  return 1;
}).then(process.exit);
