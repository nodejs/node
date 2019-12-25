'use strict';

require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer().listen(0, connectToServer);

function connectToServer() {
  const client = net.createConnection(this.address().port, () => {
    assert.throws(() => client.write(1337),
                  {
                    code: 'ERR_INVALID_ARG_TYPE',
                    name: 'TypeError'
                  });

    client.destroy();
  })
  .on('close', () => server.close());
}
