'use strict';
const common = require('../common');
const http = require('http');

const options = {
  method: 'GET',
  port: undefined,
  host: '127.0.0.1',
  path: '/'
};

const server = http.createServer();

server.listen(0, options.host, function() {
  options.port = this.address().port;
  const req = http.request(options);
  req.on('error', function() {
    // this space is intentionally left blank
  });
  req.on('close', common.mustCall(() => server.close()));

  req.setTimeout(1);
  req.on('timeout', common.mustCall(() => {
    req.end(() => {
      setTimeout(() => {
        req.destroy();
      }, 100);
    });
  }));
});
