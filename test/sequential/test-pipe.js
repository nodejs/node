'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const webPort = common.PORT;
const tcpPort = webPort + 1;
const bufferSize = 5 * 1024 * 1024;

let listenCount = 0;
let gotThanks = false;
let tcpLengthSeen = 0;


/*
 * 5MB of random buffer.
 */
const buffer = Buffer.allocUnsafe(bufferSize);
for (let i = 0; i < buffer.length; i++) {
  buffer[i] = parseInt(Math.random() * 10000) % 256;
}


const web = http.Server(common.mustCall((req, res) => {
  web.close();

  const socket = net.Stream();
  socket.connect(tcpPort);

  socket.on('connect', common.mustCall(() => {}));

  req.pipe(socket);

  req.on('end', common.mustCall(() => {
    res.writeHead(200);
    res.write('thanks');
    res.end();
  }));

  req.connection.on('error', (e) => {
    assert.ifError(e);
  });
}));

web.listen(webPort, startClient);


const tcp = net.Server(common.mustCall((s) => {
  tcp.close();

  let i = 0;

  s.on('data', (d) => {
    tcpLengthSeen += d.length;
    for (let j = 0; j < d.length; j++) {
      assert.strictEqual(buffer[i], d[j]);
      i++;
    }
  });

  s.on('end', common.mustCall(() => {
    s.end();
  }));

  s.on('error', (e) => {
    assert.ifError(e);
  });
}));

tcp.listen(tcpPort, startClient);

function startClient() {
  listenCount++;
  if (listenCount < 2) return;

  const req = http.request({
    port: common.PORT,
    method: 'GET',
    path: '/',
    headers: { 'content-length': buffer.length }
  }, common.mustCall((res) => {
    res.setEncoding('utf8');
    res.on('data', common.mustCall((string) => {
      assert.strictEqual('thanks', string);
      gotThanks = true;
    }));
  }));
  req.write(buffer);
  req.end();
}

process.on('exit', () => {
  assert.ok(gotThanks);
  assert.strictEqual(bufferSize, tcpLengthSeen);
});
