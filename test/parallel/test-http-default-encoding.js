'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

const expected = 'This is a unicode text: سلام';
let result = '';

const server = http.Server((req, res) => {
  req.setEncoding('utf8');
  req.on('data', (chunk) => {
    result += chunk;
  }).on('end', () => {
    server.close();
    res.writeHead(200);
    res.end('hello world\n');
  });

});

server.listen(0, function() {
  http.request({
    port: this.address().port,
    path: '/',
    method: 'POST'
  }, (res) => {
    console.log(res.statusCode);
    res.resume();
  }).on('error', (e) => {
    console.log(e.message);
    process.exit(1);
  }).end(expected);
});

process.on('exit', () => {
  assert.strictEqual(expected, result);
});
