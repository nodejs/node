'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');


// With only a callback, server should get a port assigned by the OS
{
  const server = net.createServer(common.mustNotCall());

  server.listen(common.mustCall(function() {
    assert.ok(server.address().port > 100);
    server.close();
  }));
}

// No callback to listen(), assume we can bind in 100 ms
{
  const server = net.createServer(common.mustNotCall());

  server.listen(common.PORT);

  setTimeout(function() {
    const address = server.address();
    assert.strictEqual(address.port, common.PORT);

    if (address.family === 6)
      assert.strictEqual(server._connectionKey, `6::::${address.port}`);
    else
      assert.strictEqual(server._connectionKey, `4:0.0.0.0:${address.port}`);

    server.close();
  }, 100);
}

// Callback to listen()
{
  const server = net.createServer(common.mustNotCall());

  server.listen(common.PORT + 1, common.mustCall(function() {
    assert.strictEqual(server.address().port, common.PORT + 1);
    server.close();
  }));
}

// Backlog argument
{
  const server = net.createServer(common.mustNotCall());

  server.listen(common.PORT + 2, '0.0.0.0', 127, common.mustCall(function() {
    assert.strictEqual(server.address().port, common.PORT + 2);
    server.close();
  }));
}

// Backlog argument without host argument
{
  const server = net.createServer(common.mustNotCall());

  server.listen(common.PORT + 3, 127, common.mustCall(function() {
    assert.strictEqual(server.address().port, common.PORT + 3);
    server.close();
  }));
}
