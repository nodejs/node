'use strict';

const {
  hasCrypto,
  mustCall,
  skip
} = require('../common');
if (!hasCrypto)
  skip('missing crypto');

const {
  deepStrictEqual
} = require('assert');
const {
  createServer,
  connect
} = require('http2');

const check = Buffer.from([ 1, 2, 3, 4, 5, 6, 7, 8 ]);

const server = createServer();
server.on('stream', mustCall((stream) => {
  stream.respond();
  stream.end('ok');
}));
server.on('session', mustCall((session) => {
  session.on('ping', mustCall((payload) => {
    deepStrictEqual(check, payload);
  }));
  session.ping(check, mustCall());
}));
server.listen(0, mustCall(() => {
  const client = connect(`http://localhost:${server.address().port}`);

  client.on('ping', mustCall((payload) => {
    deepStrictEqual(check, payload);
  }));
  client.on('connect', mustCall(() => {
    client.ping(check, mustCall());
  }));

  const req = client.request();
  req.resume();
  req.on('close', mustCall(() => {
    client.close();
    server.close();
  }));
}));
