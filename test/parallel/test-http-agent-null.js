'use strict';
const common = require('../common');
const http = require('http');

const server = http.createServer(common.mustCall((req, res) => {
  res.end();
})).listen(0, common.mustCall(() => {
  const options = {
    agent: null,
    port: server.address().port
  };
  http.get(options, common.mustCall((res) => {
    res.resume();
    server.close();
  }));
}));
