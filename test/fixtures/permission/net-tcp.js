'use strict';

const common = require('../../common');
const assert = require('assert');
const net = require('net');

const host = process.env.HOST || 'localhost';
const port = parseInt(process.env.PORT, 10);

{
  const client = net.connect({
    port,
    host,
  });

  client.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));

  client.on('connect', common.mustNotCall('TCP connection should be blocked'));
}

{
  const client = net.connect({
    port,
    host: '127.0.0.1',
  });

  client.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
    assert.strictEqual(err.syscall, 'connect');
  }));

  client.on('connect', common.mustNotCall('TCP connection should be blocked'));
}
