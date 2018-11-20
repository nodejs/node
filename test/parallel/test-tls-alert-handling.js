'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

if (!common.opensslCli)
  common.skip('node compiled without OpenSSL CLI');

const net = require('net');
const tls = require('tls');
const fixtures = require('../common/fixtures');

let clientClosed = false;
let errorReceived = false;
function canCloseServer() {
  return clientClosed && errorReceived;
}

function loadPEM(n) {
  return fixtures.readKey(`${n}.pem`, 'utf-8');
}

const opts = {
  key: loadPEM('agent2-key'),
  cert: loadPEM('agent2-cert')
};

const max_iter = 20;
let iter = 0;

const errorHandler = common.mustCall(() => {
  errorReceived = true;
  if (canCloseServer())
    server.close();
});
const server = tls.createServer(opts, common.mustCall(function(s) {
  s.pipe(s);
  s.on('error', errorHandler);
}, 2));

server.listen(0, common.mustCall(function() {
  sendClient();
}));


function sendClient() {
  const client = tls.connect(server.address().port, {
    rejectUnauthorized: false
  });
  client.on('data', common.mustCall(function() {
    if (iter++ === 2) sendBADTLSRecord();
    if (iter < max_iter) {
      client.write('a');
      return;
    }
    client.end();
  }, max_iter));
  client.write('a');
  client.on('error', common.mustNotCall());
  client.on('close', common.mustCall(function() {
    clientClosed = true;
    if (canCloseServer())
      server.close();
  }));
}


function sendBADTLSRecord() {
  const BAD_RECORD = Buffer.from([0xff, 0xff, 0xff, 0xff, 0xff, 0xff]);
  const socket = net.connect(server.address().port);
  const client = tls.connect({
    socket: socket,
    rejectUnauthorized: false
  }, common.mustCall(function() {
    socket.write(BAD_RECORD);
    socket.end();
  }));
  client.on('error', common.mustCall());
}
