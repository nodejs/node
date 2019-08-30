'use strict';
const common = require('../common');
const http = require('http');
const { finished } = require('stream')

{
  // abort before finished

  const server = http.createServer(function(req, res) {
    res.write('asd')
  });

  server.listen(0, common.mustCall(function() {
    const req = http.request({
      port: this.address().port
    })
    .on('response', res => {
      res.on('readable', () => (
        res.destroy()
      ));
      finished(res, common.mustCall(() => {
        server.close();
      }));
    })
    .end();
  }));
}
