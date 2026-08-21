'use strict';

const common = require('../common');
const http = require('http');

// Fix for https://github.com/nodejs/node/issues/14368

const server = http.createServer(handle);

function handle(req, res) {
  res.on('error', common.mustNotCall());

  res.write('hello');
  res.end();

  setImmediate(common.mustCall(() => {
    res.write('world', common.mustCall((err) => {
      common.expectsError({
        code: 'ERR_STREAM_WRITE_AFTER_END',
        name: 'Error'
      })(err);
      server.close();
    }));
  }));
}

server.listen(0, common.mustCall(() => {
  http.get(`http://localhost:${server.address().port}`);
}));
