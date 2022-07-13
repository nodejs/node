'use strict';
const common = require('../common');
const http = require('http');
const { finished } = require('stream');

{
  // Test abort before finished.

  const server = http.createServer(function(req, res) {
    res.write('asd');
  });

  server.listen(0, common.mustCall(function() {
    http.request({
      port: this.address().port
    })
    .on('response', (res) => {
      res.on('readable', () => {
        res.destroy();
      });
      finished(res, common.mustCall(() => {
        server.close();
      }));
    })
    .end();
  }));
}
