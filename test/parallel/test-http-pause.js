'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

const expectedServer = 'Request Body from Client';
let resultServer = '';
const expectedClient = 'Response Body from Server';
let resultClient = '';

const server = http.createServer((req, res) => {
  console.error('pause server request');
  req.pause();
  setTimeout(() => {
    console.error('resume server request');
    req.resume();
    req.setEncoding('utf8');
    req.on('data', (chunk) => {
      resultServer += chunk;
    });
    req.on('end', () => {
      console.error(resultServer);
      res.writeHead(200);
      res.end(expectedClient);
    });
  }, 100);
});

server.listen(0, function() {
  const req = http.request({
    port: this.address().port,
    path: '/',
    method: 'POST'
  }, (res) => {
    console.error('pause client response');
    res.pause();
    setTimeout(() => {
      console.error('resume client response');
      res.resume();
      res.on('data', (chunk) => {
        resultClient += chunk;
      });
      res.on('end', () => {
        console.error(resultClient);
        server.close();
      });
    }, 100);
  });
  req.end(expectedServer);
});

process.on('exit', () => {
  assert.strictEqual(expectedServer, resultServer);
  assert.strictEqual(expectedClient, resultClient);
});
