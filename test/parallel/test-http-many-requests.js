'use strict';

const common = require('../common');
const http = require('http');
const Countdown = require('../common/countdown');

const NUM_REQ = 128

const agent = new http.Agent({ keepAlive: true });
const countdown = new Countdown(NUM_REQ, () => server.close());

const server = http.createServer(common.mustCall(function(req, res) {
  res.setHeader('content-type', 'application/json; charset=utf-8')
  res.end(JSON.stringify({ hello: 'world' }))
}, NUM_REQ)).listen(0, function() {
  for (let i = 0; i < NUM_REQ; ++i) {
    const req = http.request({
      port: server.address().port,
      agent,
      method: 'GET'
    }, function(res) {
      res.resume();
      res.on('end', () => {
        countdown.dec();
      })
    })
    .end();
  }
});

process.on('warning', common.mustNotCall());
