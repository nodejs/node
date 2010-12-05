var common = require('../common');
var assert = require('assert');
var http = require('http');
var url = require('url');

var PROXY_PORT = common.PORT;
var BACKEND_PORT = common.PORT + 1;

var cookies = [
  'session_token=; path=/; expires=Sun, 15-Sep-2030 13:48:52 GMT',
  'prefers_open_id=; path=/; expires=Thu, 01-Jan-1970 00:00:00 GMT'
];

var headers = {'content-type': 'text/plain',
                'set-cookie': cookies,
                'hello': 'world' };

var backend = http.createServer(function(req, res) {
  common.debug('backend request');
  res.writeHead(200, headers);
  res.write('hello world\n');
  res.end();
});

var proxy_client = http.createClient(BACKEND_PORT);
var proxy = http.createServer(function(req, res) {
  common.debug('proxy req headers: ' + JSON.stringify(req.headers));
  var proxy_req = proxy_client.request(url.parse(req.url).pathname);
  proxy_req.end();
  proxy_req.addListener('response', function(proxy_res) {

    common.debug('proxy res headers: ' + JSON.stringify(proxy_res.headers));

    assert.equal('world', proxy_res.headers['hello']);
    assert.equal('text/plain', proxy_res.headers['content-type']);
    assert.deepEqual(cookies, proxy_res.headers['set-cookie']);

    res.writeHead(proxy_res.statusCode, proxy_res.headers);

    proxy_res.addListener('data', function(chunk) {
      res.write(chunk);
    });

    proxy_res.addListener('end', function() {
      res.end();
      common.debug('proxy res');
    });
  });
});

var body = '';

var nlistening = 0;
function startReq() {
  nlistening++;
  if (nlistening < 2) return;

  var client = http.createClient(PROXY_PORT);
  var req = client.request('/test');
  common.debug('client req');
  req.addListener('response', function(res) {
    common.debug('got res');
    assert.equal(200, res.statusCode);

    assert.equal('world', res.headers['hello']);
    assert.equal('text/plain', res.headers['content-type']);
    assert.deepEqual(cookies, res.headers['set-cookie']);

    res.setEncoding('utf8');
    res.addListener('data', function(chunk) { body += chunk; });
    res.addListener('end', function() {
      proxy.close();
      backend.close();
      common.debug('closed both');
    });
  });
  req.end();
}

common.debug('listen proxy');
proxy.listen(PROXY_PORT, startReq);

common.debug('listen backend');
backend.listen(BACKEND_PORT, startReq);

process.addListener('exit', function() {
  assert.equal(body, 'hello world\n');
});
