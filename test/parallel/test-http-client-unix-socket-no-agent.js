'use strict';
const common = require('../common');
const Countdown = require('../common/countdown');

const http = require('http');
const { createConnection } = require('net');

const server = http.createServer((req, res) => {
  res.end();
});

const countdown = new Countdown(2, () => {
  server.close();
});

common.refreshTmpDir();

server.listen(common.PIPE, common.mustCall(() => {
  http.get({ createConnection, socketPath: common.PIPE }, onResponse);
  http.get({ agent: 0, socketPath: common.PIPE }, onResponse);
}));

function onResponse(res) {
  res.on('end', () => {
    countdown.dec();
  });
  res.resume();
}
