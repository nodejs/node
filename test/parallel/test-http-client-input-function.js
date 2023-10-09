'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');

{
  const server = http.createServer(common.mustCall((req, res) => {
    res.writeHead(200);
    res.end('hello world');
  })).listen(8080, '127.0.0.1');

  const req = new http.ClientRequest('http://127.0.0.1:8080', common.mustCall((response) => {
    let body = '';
    response.setEncoding('utf8');
    response.on('data', (chunk) => {
      body += chunk;
    });

    response.on('end', common.mustCall(() => {
      assert.strictEqual(body, 'hello world');
      server.close();
    }));
  }));

  req.end();
}
