'use strict';

const common = require('../common');
if (common.isWindows)
  common.skip('Unix-specific test');

const assert = require('assert');
const net = require('net');

common.refreshTmpDir();

const server = net.createServer((connection) => {
  connection.on('error', (err) => {
    throw err;
  });

  const writev = connection._writev.bind(connection);
  connection._writev = common.mustCall(writev);

  connection.cork();
  connection.write('pi');
  connection.write('ng');
  connection.end();
});

server.on('error', (err) => {
  throw err;
});

server.listen(common.PIPE, () => {
  const client = net.connect(common.PIPE);

  client.on('error', (err) => {
    throw err;
  });

  client.on('data', common.mustCall((data) => {
    assert.strictEqual(data.toString(), 'ping');
  }));

  client.on('end', () => {
    server.close();
  });
});
