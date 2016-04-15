'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const server = net.createServer();

server.on('error', () => {
  console.error('server had an error');
});

assert.throws(function() {
  server.listen(common.PORT).listen(common.PORT+1);
}, /listen\(\) cannot be called multiple times/i);
  
process.on('exit', () => {
  // Is this needed? assert.equal(server.address().port, 8080);
  const key = server._connectionKey;
  assert.equal(key.substr(key.lastIndexOf(':')+1), common.PORT);
});

server.close();
