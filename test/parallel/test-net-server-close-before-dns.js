'use strict';
const common = require('../common');
const net = require('net');

net.createServer(() => {}).listen(common.PORT, 'localhost', () => {
  net.connect(common.PORT, () => {
    throw new Error('a connection was made');
  });
}).close();
