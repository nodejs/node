'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const url = require('url');

var responses_sent = 0;
var responses_recvd = 0;
var body0 = '';
var body1 = '';

const server = http.Server(function(req, res) {
  if (responses_sent === 0) {
    assert.strictEqual('GET', req.method);
    assert.strictEqual('/hello', url.parse(req.url).pathname);

    console.dir(req.headers);
    assert.strictEqual(true, 'accept' in req.headers);
    assert.strictEqual('*/*', req.headers['accept']);

    assert.strictEqual(true, 'foo' in req.headers);
    assert.strictEqual('bar', req.headers['foo']);
  }

  if (responses_sent === 1) {
    assert.strictEqual('POST', req.method);
    assert.strictEqual('/world', url.parse(req.url).pathname);
    this.close();
  }

  req.on('end', function() {
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.write('The path was ' + url.parse(req.url).pathname);
    res.end();
    responses_sent += 1;
  });
  req.resume();

  //assert.equal('127.0.0.1', res.connection.remoteAddress);
});
server.listen(0);

server.on('listening', function() {
  const agent = new http.Agent({ port: this.address().port, maxSockets: 1 });
  http.get({
    port: this.address().port,
    path: '/hello',
    headers: {'Accept': '*/*', 'Foo': 'bar'},
    agent: agent
  }, function(res) {
    assert.strictEqual(200, res.statusCode);
    responses_recvd += 1;
    res.setEncoding('utf8');
    res.on('data', function(chunk) { body0 += chunk; });
    console.error('Got /hello response');
  });

  setTimeout(function() {
    const req = http.request({
      port: server.address().port,
      method: 'POST',
      path: '/world',
      agent: agent
    }, function(res) {
      assert.strictEqual(200, res.statusCode);
      responses_recvd += 1;
      res.setEncoding('utf8');
      res.on('data', function(chunk) { body1 += chunk; });
      console.error('Got /world response');
    });
    req.end();
  }, 1);
});

process.on('exit', function() {
  console.error('responses_recvd: ' + responses_recvd);
  assert.strictEqual(2, responses_recvd);

  console.error('responses_sent: ' + responses_sent);
  assert.strictEqual(2, responses_sent);

  assert.strictEqual('The path was /hello', body0);
  assert.strictEqual('The path was /world', body1);
});
