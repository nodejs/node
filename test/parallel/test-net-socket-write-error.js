'use strict';

const common = require('../common');
const net = require('net');

const server = net.createServer().listen(common.PORT, connectToServer);

function connectToServer() {
  const client = net.createConnection(common.PORT, () => {
    common.throws(() => {
      client.write(1337);
    }, {code: 'INVALIDARG'});

    client.end();
  })
  .on('end', () => server.close());
}
