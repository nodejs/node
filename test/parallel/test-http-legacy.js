'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const url = require('url');

let responses_sent = 0;
let body0 = '';
let body1 = '';

const server = http.createServer(function(req, res) {
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
});

server.listen(0, common.mustCall(function() {
  const client = http.createClient(this.address().port);
  const req = client.request('/hello', {'Accept': '*/*', 'Foo': 'bar'});
  setTimeout(function() {
    req.end();
  }, 100);
  req.on('response', common.mustCall(function(res) {
    assert.strictEqual(200, res.statusCode);
    res.setEncoding('utf8');
    res.on('data', function(chunk) { body0 += chunk; });
    console.error('Got /hello response');
  }));

  setTimeout(common.mustCall(function() {
    const req = client.request('POST', '/world');
    req.end();
    req.on('response', common.mustCall(function(res) {
      assert.strictEqual(200, res.statusCode);
      res.setEncoding('utf8');
      res.on('data', function(chunk) { body1 += chunk; });
      console.error('Got /world response');
    }));
  }), 1);
}));

process.on('exit', function() {
  console.error('responses_sent: ' + responses_sent);
  assert.strictEqual(2, responses_sent);

  assert.strictEqual('The path was /hello', body0);
  assert.strictEqual('The path was /world', body1);
});
