'use strict';

const common = require('../common');
const { spawn } = require('child_process');
const net = require('net');

const server = net.createServer((conn) => {
  conn.on('close', common.mustCall());

  spawn(process.execPath, ['-v'], {
    stdio: ['ignore', conn, 'ignore']
  }).on('close', common.mustCall());
}).listen(common.PIPE, () => {
  const client = net.connect(common.PIPE, common.mustCall());
  client.on('data', () => {
    client.end(() => {
      server.close();
    });
  });
});
