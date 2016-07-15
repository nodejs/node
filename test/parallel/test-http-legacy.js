'use strict';
const common = require('../common');
var assert = require('assert');
var http = require('http');
var url = require('url');

var responses_sent = 0;
var body0 = '';
var body1 = '';

var server = http.createServer(function(req, res) {
  if (responses_sent == 0) {
    assert.equal('GET', req.method);
    assert.equal('/hello', url.parse(req.url).pathname);

    console.dir(req.headers);
    assert.equal(true, 'accept' in req.headers);
    assert.equal('*/*', req.headers['accept']);

    assert.equal(true, 'foo' in req.headers);
    assert.equal('bar', req.headers['foo']);
  }

  if (responses_sent == 1) {
    assert.equal('POST', req.method);
    assert.equal('/world', url.parse(req.url).pathname);
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
  var client = http.createClient(this.address().port);
  var req = client.request('/hello', {'Accept': '*/*', 'Foo': 'bar'});
  setTimeout(function() {
    req.end();
  }, 100);
  req.on('response', common.mustCall(function(res) {
    assert.equal(200, res.statusCode);
    res.setEncoding('utf8');
    res.on('data', function(chunk) { body0 += chunk; });
    console.error('Got /hello response');
  }));

  setTimeout(common.mustCall(function() {
    var req = client.request('POST', '/world');
    req.end();
    req.on('response', common.mustCall(function(res) {
      assert.equal(200, res.statusCode);
      res.setEncoding('utf8');
      res.on('data', function(chunk) { body1 += chunk; });
      console.error('Got /world response');
    }));
  }), 1);
}));

process.on('exit', function() {
  console.error('responses_sent: ' + responses_sent);
  assert.equal(2, responses_sent);

  assert.equal('The path was /hello', body0);
  assert.equal('The path was /world', body1);
});
