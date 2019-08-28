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

{
  // Test abort before finished.

  const server = http.createServer(function(req, res) {
  });

  server.listen(0, common.mustCall(function() {
    const req = http.request({
      port: this.address().port
    }, common.mustNotCall());
    req.abort();
    finished(req, common.mustCall(() => {
      server.close();
    }));
  }));
}

{
  // Test abort after request.

  const server = http.createServer(function(req, res) {
  });

  server.listen(0, common.mustCall(function() {
    const req = http.request({
      port: this.address().port
    }).end();
    finished(req, (err) => {
      common.expectsError({
        type: Error,
        code: 'ERR_STREAM_PREMATURE_CLOSE'
      })(err)
      server.close();
    });
    req.abort();
  }));
}
