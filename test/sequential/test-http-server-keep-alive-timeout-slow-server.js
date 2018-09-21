'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCall((req, res) => {
  if (req.url === '/first') {
    res.end('ok');
    return;
  }
  setTimeout(() => {
    res.end('ok');
  }, common.platformTimeout(500));
}, 2));

server.keepAliveTimeout = common.platformTimeout(200);

const agent = new http.Agent({
  keepAlive: true,
  maxSockets: 1
});

function request(path, callback) {
  const port = server.address().port;
  const req = http.request({ agent, path, port }, common.mustCall((res) => {
    assert.strictEqual(res.statusCode, 200);

    res.setEncoding('utf8');

    let result = '';
    res.on('data', (chunk) => {
      result += chunk;
    });

    res.on('end', common.mustCall(() => {
      assert.strictEqual(result, 'ok');
      callback();
    }));
  }));
  req.end();
}

server.listen(0, common.mustCall(() => {
  request('/first', () => {
    request('/second', () => {
      server.close();
    });
  });
}));
