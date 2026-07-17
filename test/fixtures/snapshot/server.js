'use strict';

const net = require('net');
const {
  setDeserializeMainFunction
} = require('v8').startupSnapshot;
const dc = require('diagnostics_channel');

const echoServer = net.Server(function(connection) {
  connection.on('data', function(chunk) {
    connection.write(chunk);
  });
  connection.on('end', function() {
    connection.end();
  });
});

const kNumChars = 256;
const buffer = new Uint8Array(kNumChars);
for (let i = 0; i < kNumChars; ++i) {
  buffer[i] = i;
}

let recv = '';

echoServer.on('listening', function() {
  const port = this.address().port;
  console.log(`server port`, port);
  const c = net.createConnection({ host: '127.0.0.1', port });

  c.on('data', function(chunk) {
    recv += chunk.toString('latin1');

    if (recv.length === buffer.length) {
      c.end();
    }
  });

  c.on('connect', function() {
    c.write(buffer);
  });

  c.on('close', function() {
    console.log(`recv.length: ${recv.length}`);
    echoServer.close();
  });

});

dc.subscribe('net.server.socket', (({ socket }) => {
  console.log(`From server diagnostics channel:`, socket.localPort);
}));

dc.subscribe('net.client.socket', (({ socket }) => {
  console.log(`From client diagnostics channel`);
}));

setDeserializeMainFunction(() => {
  echoServer.listen(0);
});
