'use strict';
const common = require('../common');
const http = require('http');

const server = http.createServer(common.mustCall((req, res) => {
  req.pipe(res);
  res.on('error', common.mustNotCall());
  res.on('close', common.mustCall(() => {
    res.end('asd');
    process.nextTick(() => {
      server.close();
    });
  }));
})).listen(0, () => {
  http
    .request({
      port: server.address().port,
      method: 'PUT'
    })
    .on('response', (res) => {
      res.destroy();
    })
    .write('asd');
});
