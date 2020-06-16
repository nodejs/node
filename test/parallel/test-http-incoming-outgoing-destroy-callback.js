'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

{
  const server = http.createServer(common.mustCall((req, res) => {
    res.destroy('xyz', common.mustCall((err) => {
      assert.strictEqual(err, 'xyz');
    }));
    server.close();
  }));
  server.listen(0);
  server.on('listening', common.mustCall(() => {
    const clientRequest = http.request({
      port: server.address().port,
      method: 'GET',
      path: '/'
    });
    clientRequest.on('error', common.mustCall());
    clientRequest.end();
  }));
}

{
  const server = http.createServer(common.mustCall((req, res) => {
    req.destroy('xyz', common.mustCall((err) => {
      assert.strictEqual(err, 'xyz');
    }));
    server.close();
  }));
  server.listen(0);
  server.on('listening', common.mustCall(() => {
    const clientRequest = http.request({
      port: server.address().port,
      method: 'GET',
      path: '/'
    });
    clientRequest.on('error', common.mustCall());
    clientRequest.end();
  }));
}

{
  const server = http.createServer(common.mustCall((req, res) => {
    res.end();
  }));
  server.listen(0);
  server.on('listening', common.mustCall(() => {
    const clientRequest = http.request({
      port: server.address().port,
      method: 'GET',
      path: '/'
    });
    clientRequest.on('response', common.mustCall((res) => {
      res.destroy('zyx', common.mustCall((err) => {
        assert.strictEqual(err, 'zyx');
      }));
      server.close();
    }));
    clientRequest.on('error', common.mustCall());
    clientRequest.end();
  }));
}
